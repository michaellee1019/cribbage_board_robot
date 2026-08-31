#include <OtaUpdate.hpp>
#include <OtaTransferRules.hpp>

#include <Update.h>

#include <array>
#include <cstdlib>
#include <cstring>

namespace {
constexpr char kOtaServiceUuid[] = "c6a861b0-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kOtaControlUuid[] = "c6a861b1-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kOtaDataUuid[] = "c6a861b2-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kOtaStatusUuid[] = "c6a861b3-2f9d-46bc-9a23-bb9c89a519be";
// A full set is updated sequentially over BLE. Ten minutes gives every
// locally armed board time to receive the image without changing radio duty.
constexpr uint32_t kArmWindowMs = 10 * 60 * 1000;
constexpr uint32_t kRestartDelayMs = 750;
constexpr uint16_t kChunkCapacity = 512;
constexpr uint32_t kTransferTimeoutMs = 30000;
constexpr uint16_t kNoConnection = BLE_HS_CONN_HANDLE_NONE;
}  // namespace

class OtaControlCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit OtaControlCallbacks(OtaUpdate& owner) : owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
        owner.handleControl(characteristic->getValue(), connection.getConnHandle());
    }

private:
    OtaUpdate& owner;
};

class OtaDataCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit OtaDataCallbacks(OtaUpdate& owner) : owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
        owner.handleData(characteristic->getValue(), connection.getConnHandle());
    }

private:
    OtaUpdate& owner;
};

OtaUpdate::OtaUpdate()
    : statusCharacteristic(nullptr),
      server(nullptr),
      armUntilMs(0),
      expectedBytes(0),
      receivedBytes(0),
      writerConnectionHandle(kNoConnection),
      lastProgressMs(0),
      timeoutDisconnectRequested(false),
      restartAtMs(0),
      armed(false),
      writing(false),
      controlCallbacks(),
      dataCallbacks() {}

OtaUpdate::~OtaUpdate() = default;

void OtaUpdate::setup(NimBLEServer* server) {
    this->server = server;
    NimBLEService* service = server->createService(kOtaServiceUuid);
    NimBLECharacteristic* control = service->createCharacteristic(
        kOtaControlUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, 64);
    NimBLECharacteristic* data = service->createCharacteristic(
        kOtaDataUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, kChunkCapacity);
    statusCharacteristic = service->createCharacteristic(
        kOtaStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 64);
    controlCallbacks = std::make_unique<OtaControlCallbacks>(*this);
    dataCallbacks = std::make_unique<OtaDataCallbacks>(*this);
    control->setCallbacks(controlCallbacks.get());
    data->setCallbacks(dataCallbacks.get());
    setStatus("IDLE");
}

void OtaUpdate::arm() {
    if (writing.load()) {
        return;
    }
    armed.store(true);
    armUntilMs.store(millis() + kArmWindowMs);
    setStatus("ARMED");
}

bool OtaUpdate::isArmed() const {
    return armed.load() &&
           static_cast<int32_t>(armUntilMs.load() - millis()) > 0;
}

bool OtaUpdate::isWriting() const {
    return writing.load();
}

void OtaUpdate::setStatus(const char* status) {
    if (statusCharacteristic == nullptr) {
        return;
    }
    statusCharacteristic->setValue(status);
    statusCharacteristic->notify();
}

void OtaUpdate::abort(const char* status) {
    if (writing.load()) {
        Update.abort();
    }
    writing.store(false);
    armed.store(false);
    expectedBytes = 0;
    receivedBytes = 0;
    writerConnectionHandle.store(kNoConnection);
    lastProgressMs.store(0);
    timeoutDisconnectRequested.store(false);
    setStatus(status);
}

void OtaUpdate::handleControl(const NimBLEAttValue& value, uint16_t connectionHandle) {
    const String command = String(value.c_str()).substring(0, value.length());
    if (command == "ABORT") {
        if (writing.load() && !scorebot::otaTransferOwnedBy(
                                  true, writerConnectionHandle.load(), connectionHandle)) {
            setStatus("ERR:OWNER");
            return;
        }
        abort("ABORTED");
        return;
    }
    if (command == "COMMIT") {
        if (!writing.load()) {
            setStatus("ERR:NOT_WRITING");
            return;
        }
        if (!scorebot::otaTransferOwnedBy(
                true, writerConnectionHandle.load(), connectionHandle)) {
            setStatus("ERR:OWNER");
            return;
        }
        if (receivedBytes != expectedBytes || !Update.end(true)) {
            abort("ERR:COMMIT");
            return;
        }
        writing.store(false);
        armed.store(false);
        writerConnectionHandle.store(kNoConnection);
        lastProgressMs.store(0);
        setStatus("DONE");
        restartAtMs.store(millis() + kRestartDelayMs);
        return;
    }
    if (!command.startsWith("START:")) {
        setStatus("ERR:COMMAND");
        return;
    }
    if (!isArmed()) {
        setStatus("ERR:NOT_ARMED");
        return;
    }
    if (writing.load()) {
        setStatus("ERR:BUSY");
        return;
    }
    const uint32_t size = static_cast<uint32_t>(strtoul(command.c_str() + 6, nullptr, 10));
    if (size == 0 || !Update.begin(size, U_FLASH)) {
        abort("ERR:START");
        return;
    }
    expectedBytes = size;
    receivedBytes = 0;
    writerConnectionHandle.store(connectionHandle);
    lastProgressMs.store(millis());
    timeoutDisconnectRequested.store(false);
    writing.store(true);
    setStatus("READY");
}

void OtaUpdate::handleData(const NimBLEAttValue& value, uint16_t connectionHandle) {
    if (!writing.load()) {
        setStatus("ERR:NOT_WRITING");
        return;
    }
    if (!scorebot::otaTransferOwnedBy(
            writing.load(), writerConnectionHandle.load(), connectionHandle)) {
        setStatus("ERR:OWNER");
        return;
    }
    const size_t length = value.length();
    std::array<uint8_t, kChunkCapacity> chunk{};
    if (length > chunk.size() || !scorebot::otaCanAppend(
                                     writing.load(), writerConnectionHandle.load(), connectionHandle,
                                     receivedBytes, expectedBytes, length)) {
        abort("ERR:WRITE");
        return;
    }
    // Espressif's Update API accepts a mutable pointer even though it only
    // consumes bytes. Copy the const BLE value instead of casting it away.
    std::memcpy(chunk.data(), value.data(), length);
    if (Update.write(chunk.data(), length) != length) {
        abort("ERR:WRITE");
        return;
    }
    receivedBytes += length;
    lastProgressMs.store(millis());
}

void OtaUpdate::onDisconnected(uint16_t connectionHandle) {
    if (scorebot::otaTransferOwnedBy(
            writing.load(), writerConnectionHandle.load(), connectionHandle)) {
        abort(timeoutDisconnectRequested.load() ? "ERR:TIMEOUT" : "ABORTED");
    }
}

void OtaUpdate::loop() {
    if (server != nullptr && scorebot::otaTransferTimedOut(
                              writing.load(), millis(), lastProgressMs.load(), kTransferTimeoutMs) &&
        !timeoutDisconnectRequested.exchange(true)) {
        if (!server->disconnect(writerConnectionHandle.load())) {
            timeoutDisconnectRequested.store(false);
        }
    }
    if (armed.load() && !writing.load() && !isArmed()) {
        armed.store(false);
        setStatus("IDLE");
    }
    const uint32_t restartAt = restartAtMs.load();
    if (restartAt != 0 && static_cast<int32_t>(millis() - restartAt) >= 0) {
        ESP.restart();
    }
}
