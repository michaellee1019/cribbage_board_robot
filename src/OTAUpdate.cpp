#include <OtaUpdate.hpp>
#include <ErrorHandler.hpp>
#include <OtaTransferRules.hpp>
#include <utils.hpp>

#include <Update.h>

#include <array>
#include <cstdlib>
#include <cstring>

namespace {
constexpr char kOtaControlUuid[] = "c6a861b1-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kOtaDataUuid[] = "c6a861b2-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kOtaStatusUuid[] = "c6a861b3-2f9d-46bc-9a23-bb9c89a519be";
// A full set is updated sequentially over BLE. Ten minutes gives every
// locally armed board time to receive the image without changing radio duty.
constexpr uint32_t kArmWindowMs = 10 * 60 * 1000;
constexpr uint32_t kRestartDelayMs = 750;
constexpr uint32_t kTransferTimeoutMs = 30000;
constexpr uint16_t kNoConnection = BLE_HS_CONN_HANDLE_NONE;
constexpr UBaseType_t kRequestQueueDepth = 4;
constexpr uint32_t kWorkerPollMs = 20;

class StatusGuard final {
public:
    explicit StatusGuard(SemaphoreHandle_t mutex) : mutex(mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
    ~StatusGuard() { xSemaphoreGive(mutex); }

private:
    SemaphoreHandle_t mutex;
};
}  // namespace

class OtaControlCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit OtaControlCallbacks(OtaUpdate& owner) : owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
        owner.enqueueControl(characteristic->getValue(), connection.getConnHandle());
    }

private:
    OtaUpdate& owner;
};

class OtaDataCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit OtaDataCallbacks(OtaUpdate& owner) : owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
        owner.enqueueData(characteristic->getValue(), connection.getConnHandle());
    }

private:
    OtaUpdate& owner;
};

void otaWorkerTask(void* parameter) {
    static_cast<OtaUpdate*>(parameter)->workerLoop();
}

OtaUpdate::OtaUpdate()
    : statusCharacteristic(nullptr),
      server(nullptr),
      statusMutex(nullptr),
      admissionMutex(nullptr),
      requestQueue(nullptr),
      workerTask(nullptr),
      requestQueueFault(false),
      reservedConnectionHandle(kNoConnection),
      rejectedConnections(),
      rejectedConnectionCount(0),
      transportQuiescing(false),
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

OtaUpdate::~OtaUpdate() {
    if (workerTask != nullptr) {
        vTaskDelete(workerTask);
    }
    if (requestQueue != nullptr) {
        vQueueDelete(requestQueue);
    }
    if (statusMutex != nullptr) {
        vSemaphoreDelete(statusMutex);
    }
    if (admissionMutex != nullptr) {
        vSemaphoreDelete(admissionMutex);
    }
}

void OtaUpdate::setup(NimBLEServer* server) {
    this->server = server;
    statusMutex = xSemaphoreCreateMutex();
    CHECK_POINTER(statusMutex, ErrorCode::SEMAPHORE_CREATE_FAILED, "OTA status mutex");
    admissionMutex = xSemaphoreCreateMutex();
    CHECK_POINTER(
        admissionMutex, ErrorCode::SEMAPHORE_CREATE_FAILED,
        "OTA request admission mutex");
    requestQueue = xQueueCreate(kRequestQueueDepth, sizeof(Request));
    CHECK_POINTER(requestQueue, ErrorCode::QUEUE_CREATE_FAILED, "OTA request queue");
    const BaseType_t taskResult = xTaskCreatePinnedToCore(
        otaWorkerTask, "ota-worker", 6144, this, 2, &workerTask,
        ARDUINO_RUNNING_CORE);
    CHECK_FREERTOS_RESULT(
        taskResult, ErrorCode::TASK_CREATE_FAILED, "OTA flash worker");
    NimBLEService* service = server->createService(scorebot::kOtaServiceUuid);
    CHECK_POINTER(service, ErrorCode::MEMORY_ALLOCATION_FAILED, "OTA BLE service");
    NimBLECharacteristic* control = service->createCharacteristic(
        kOtaControlUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, 64);
    NimBLECharacteristic* data = service->createCharacteristic(
        kOtaDataUuid, NIMBLE_PROPERTY::WRITE, kChunkCapacity);
    statusCharacteristic = service->createCharacteristic(
        kOtaStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 64);
    CHECK_POINTER(control, ErrorCode::MEMORY_ALLOCATION_FAILED, "OTA control characteristic");
    CHECK_POINTER(data, ErrorCode::MEMORY_ALLOCATION_FAILED, "OTA data characteristic");
    CHECK_POINTER(statusCharacteristic, ErrorCode::MEMORY_ALLOCATION_FAILED,
                  "OTA status characteristic");
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

bool OtaUpdate::isActive() const {
    return isArmed() || writing.load() || restartAtMs.load() != 0 ||
           reservedConnectionHandle.load(std::memory_order_acquire) !=
               kNoConnection;
}

bool OtaUpdate::isWriting() const {
    return writing.load();
}

void OtaUpdate::setTransportQuiescing(bool quiescing) {
    StatusGuard guard(admissionMutex);
    transportQuiescing.store(quiescing, std::memory_order_release);
}

void OtaUpdate::setStatus(const char* status, uint16_t connectionHandle) {
    if (statusCharacteristic == nullptr) {
        return;
    }
    StatusGuard guard(statusMutex);
    statusCharacteristic->setValue(status);
    if (connectionHandle == kNoConnection) {
        statusCharacteristic->notify();
    } else {
        statusCharacteristic->notify(connectionHandle);
    }
}

void OtaUpdate::abort(const char* status, uint16_t connectionHandle) {
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
    reservedConnectionHandle.store(kNoConnection, std::memory_order_release);
    if (requestQueue != nullptr) {
        xQueueReset(requestQueue);
    }
    setStatus(status, connectionHandle);
}

bool OtaUpdate::enqueueRequest(
    RequestType type, const uint8_t* data, size_t length,
    uint16_t connectionHandle) {
    if (requestQueue == nullptr || length > kChunkCapacity ||
        (length != 0 && data == nullptr)) {
        requestQueueFault.store(true, std::memory_order_release);
        return false;
    }
    Request request{};
    request.type = type;
    request.connectionHandle = connectionHandle;
    request.length = static_cast<uint16_t>(length);
    if (length != 0) {
        std::memcpy(request.payload.data(), data, length);
    }
    if (xQueueSend(requestQueue, &request, 0) != pdPASS) {
        // The worker turns this into a terminal error. Dropping one chunk and
        // continuing would otherwise create an apparently valid corrupt image.
        requestQueueFault.store(true, std::memory_order_release);
        return false;
    }
    return true;
}

void OtaUpdate::enqueueControl(
    const NimBLEAttValue& value, uint16_t connectionHandle) {
    StatusGuard admission(admissionMutex);
    if (transportQuiescing.load(std::memory_order_acquire)) {
        rejectConnection(connectionHandle);
        return;
    }
    const bool startsTransfer =
        value.length() >= 6 &&
        std::memcmp(value.data(), "START:", 6) == 0;
    uint16_t reserved = reservedConnectionHandle.load(std::memory_order_acquire);
    if (startsTransfer && reserved == kNoConnection) {
        (void)reservedConnectionHandle.compare_exchange_strong(
            reserved, connectionHandle, std::memory_order_acq_rel);
    }
    if (reservedConnectionHandle.load(std::memory_order_acquire) !=
        connectionHandle) {
        rejectConnection(connectionHandle);
        return;
    }
    (void)enqueueRequest(
        RequestType::Control, value.data(), value.length(), connectionHandle);
}

void OtaUpdate::enqueueData(
    const NimBLEAttValue& value, uint16_t connectionHandle) {
    StatusGuard admission(admissionMutex);
    if (transportQuiescing.load(std::memory_order_acquire)) {
        rejectConnection(connectionHandle);
        return;
    }
    if (reservedConnectionHandle.load(std::memory_order_acquire) !=
        connectionHandle) {
        rejectConnection(connectionHandle);
        return;
    }
    (void)enqueueRequest(
        RequestType::Data, value.data(), value.length(), connectionHandle);
}

void OtaUpdate::enqueueDisconnected(uint16_t connectionHandle) {
    if (reservedConnectionHandle.load(std::memory_order_acquire) !=
        connectionHandle) {
        return;
    }
    (void)enqueueRequest(
        RequestType::Disconnected, nullptr, 0, connectionHandle);
}

void OtaUpdate::rejectConnection(uint16_t connectionHandle) {
    // Called only while admissionMutex is held. At most four non-owner links
    // can coexist with the reserved owner under the configured BLE limit.
    for (uint8_t index = 0; index < rejectedConnectionCount; ++index) {
        if (rejectedConnections[index] == connectionHandle) {
            return;
        }
    }
    if (rejectedConnectionCount < rejectedConnections.size()) {
        rejectedConnections[rejectedConnectionCount++] = connectionHandle;
    }
}

void OtaUpdate::processControl(const Request& request) {
    if (request.length > 64) {
        setStatus("ERR:COMMAND", request.connectionHandle);
        return;
    }
    std::array<char, 65> commandBuffer{};
    std::memcpy(
        commandBuffer.data(), request.payload.data(), request.length);
    const String command(commandBuffer.data());
    const uint16_t connectionHandle = request.connectionHandle;
    if (command == "ABORT") {
        if (writing.load() && !scorebot::otaTransferOwnedBy(
                                  true, writerConnectionHandle.load(), connectionHandle)) {
            setStatus("ERR:OWNER", connectionHandle);
            return;
        }
        abort("ABORTED", connectionHandle);
        return;
    }
    if (command == "COMMIT") {
        if (!writing.load()) {
            setStatus("ERR:NOT_WRITING", connectionHandle);
            return;
        }
        if (!scorebot::otaTransferOwnedBy(
                true, writerConnectionHandle.load(), connectionHandle)) {
            setStatus("ERR:OWNER", connectionHandle);
            return;
        }
        if (receivedBytes != expectedBytes || !Update.end(true)) {
            abort("ERR:COMMIT", connectionHandle);
            return;
        }
        writing.store(false);
        armed.store(false);
        writerConnectionHandle.store(kNoConnection);
        lastProgressMs.store(0);
        setStatus("DONE", connectionHandle);
        reservedConnectionHandle.store(kNoConnection, std::memory_order_release);
        restartAtMs.store(millis() + kRestartDelayMs);
        return;
    }
    if (!command.startsWith("START:")) {
        abort("ERR:COMMAND", connectionHandle);
        return;
    }
    if (!isArmed()) {
        abort("ERR:NOT_ARMED", connectionHandle);
        return;
    }
    if (writing.load()) {
        setStatus("ERR:BUSY", connectionHandle);
        return;
    }
    const uint32_t size = static_cast<uint32_t>(strtoul(command.c_str() + 6, nullptr, 10));
    if (size == 0 || !Update.begin(size, U_FLASH)) {
        abort("ERR:START", connectionHandle);
        return;
    }
    expectedBytes = size;
    receivedBytes = 0;
    writerConnectionHandle.store(connectionHandle);
    lastProgressMs.store(millis());
    timeoutDisconnectRequested.store(false);
    writing.store(true);
    // Advertise application-level flow control to new senders. Legacy firmware
    // used READY and completed each flash write synchronously in the BLE
    // callback; READY:CREDIT tells the sender to wait for NEXT from our worker.
    setStatus("READY:CREDIT", connectionHandle);
}

void OtaUpdate::processData(Request& request) {
    const uint16_t connectionHandle = request.connectionHandle;
    if (!writing.load()) {
        setStatus("ERR:NOT_WRITING", connectionHandle);
        return;
    }
    if (!scorebot::otaTransferOwnedBy(
            writing.load(), writerConnectionHandle.load(), connectionHandle)) {
        setStatus("ERR:OWNER", connectionHandle);
        return;
    }
    const size_t length = request.length;
    if (length > request.payload.size() || !scorebot::otaCanAppend(
                                     writing.load(), writerConnectionHandle.load(), connectionHandle,
                                     receivedBytes, expectedBytes, length)) {
        abort("ERR:WRITE", connectionHandle);
        return;
    }
    if (Update.write(request.payload.data(), length) != length) {
        abort("ERR:WRITE", connectionHandle);
        return;
    }
    receivedBytes += length;
    lastProgressMs.store(millis());
    // The uploader waits for this durable-consumption credit before sending
    // another chunk, so the bounded queue cannot outrun flash.
    setStatus("NEXT", connectionHandle);
}

void OtaUpdate::onDisconnected(uint16_t connectionHandle) {
    enqueueDisconnected(connectionHandle);
}

void OtaUpdate::processDisconnected(const Request& request) {
    if (scorebot::otaTransferOwnedBy(
            writing.load(), writerConnectionHandle.load(),
            request.connectionHandle)) {
        abort(
            timeoutDisconnectRequested.load() ? "ERR:TIMEOUT" : "ABORTED",
            request.connectionHandle);
        return;
    }
    uint16_t expected = request.connectionHandle;
    if (reservedConnectionHandle.compare_exchange_strong(
            expected, kNoConnection, std::memory_order_acq_rel)) {
        setStatus(isArmed() ? "ARMED" : "IDLE");
    }
}

void OtaUpdate::workerLoop() {
    DEBUG_PRINTF("OTA: worker core=%d\n", xPortGetCoreID());
    Request request{};
    uint32_t processedRequests = 0;
    while (true) {
        if (requestQueueFault.exchange(false, std::memory_order_acq_rel)) {
            abort(
                "ERR:BUSY",
                reservedConnectionHandle.load(std::memory_order_acquire));
            continue;
        }
        if (xQueueReceive(
                requestQueue, &request,
                pdMS_TO_TICKS(kWorkerPollMs)) != pdPASS) {
            continue;
        }
        switch (request.type) {
            case RequestType::Control:
                processControl(request);
                break;
            case RequestType::Data:
                processData(request);
                break;
            case RequestType::Disconnected:
                processDisconnected(request);
                break;
        }
        ++processedRequests;
        if ((processedRequests & 0x7f) == 0) {
            DEBUG_PRINTF(
                "OTA: worker stack high-water=%u words\n",
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        }
    }
}

void OtaUpdate::loop() {
    std::array<uint16_t, 4> rejected{};
    uint8_t rejectedCount = 0;
    {
        StatusGuard admission(admissionMutex);
        rejectedCount = rejectedConnectionCount;
        for (uint8_t index = 0; index < rejectedCount; ++index) {
            rejected[index] = rejectedConnections[index];
        }
        rejectedConnectionCount = 0;
    }
    if (server != nullptr) {
        for (uint8_t index = 0; index < rejectedCount; ++index) {
            (void)server->disconnect(rejected[index]);
        }
    }
    if (server != nullptr && scorebot::otaTransferTimedOut(
                              writing.load(), millis(), lastProgressMs.load(), kTransferTimeoutMs) &&
        !timeoutDisconnectRequested.exchange(true)) {
        if (!server->disconnect(writerConnectionHandle.load())) {
            timeoutDisconnectRequested.store(false);
        }
    }
    if (armed.load() && !writing.load() && !isArmed()) {
        armed.store(false);
        reservedConnectionHandle.store(kNoConnection, std::memory_order_release);
        setStatus("IDLE");
    }
    const uint32_t restartAt = restartAtMs.load();
    if (restartAt != 0 && static_cast<int32_t>(millis() - restartAt) >= 0) {
        ESP.restart();
    }
}
