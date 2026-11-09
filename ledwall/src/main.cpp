#include <Arduino.h>
#include <NimBLEDevice.h>

#include <BleService.hpp>
#include <BoardIdentity.hpp>
#include <Protocol.hpp>

#include <cstring>

namespace {
struct StateNote {
    uint16_t length;
    char bytes[scorebot::kMaxWireMessageSize];
};

QueueHandle_t stateQueue = nullptr;

bool contains(const uint8_t* data, size_t length, const char* needle) {
    const size_t needleLength = std::strlen(needle);
    if (needleLength == 0 || needleLength > length) {
        return false;
    }
    for (size_t offset = 0; offset + needleLength <= length; ++offset) {
        if (std::memcmp(data + offset, needle, needleLength) == 0) {
            return true;
        }
    }
    return false;
}

bool isStateSnapshot(const uint8_t* data, size_t length) {
    return length > 0 && length <= scorebot::kMaxWireMessageSize &&
           (contains(data, length, "\"type\":\"state\"") ||
            contains(data, length, "\"type\": \"state\""));
}

void enqueueState(const uint8_t* data, size_t length) {
    if (stateQueue == nullptr || !isStateSnapshot(data, length)) {
        return;
    }
    StateNote note{};
    note.length = static_cast<uint16_t>(length);
    std::memcpy(note.bytes, data, length);
    if (xQueueSend(stateQueue, &note, 0) == pdPASS) {
        return;
    }
    StateNote discarded{};
    if (xQueueReceive(stateQueue, &discarded, 0) == pdPASS) {
        xQueueSend(stateQueue, &note, 0);
    }
}

class StateDownlink final : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const NimBLEAttValue value = characteristic->getValue();
        enqueueState(value.data(), value.length());
    }
};

StateDownlink downlinkCallbacks;

void drainState(void) {
    if (stateQueue == nullptr) {
        return;
    }
    StateNote note{};
    while (xQueueReceive(stateQueue, &note, 0) == pdPASS) {
        Serial.print("STATE ");
        Serial.write(reinterpret_cast<const uint8_t*>(note.bytes), note.length);
        Serial.print('\n');
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    stateQueue = xQueueCreate(2, sizeof(StateNote));
    if (stateQueue == nullptr) {
        Serial.println("STATE-WAIT queue-failed");
        return;
    }

    const uint32_t peerId = scorebot::boardIdFromEfuseMac(ESP.getEfuseMac());
    const String deviceName = String("Scorebot-") + String(peerId, HEX);
    if (!NimBLEDevice::init(deviceName.c_str())) {
        Serial.println("STATE-WAIT ble-init-failed");
        return;
    }
    NimBLEDevice::setMTU(185);
    NimBLEDevice::setPower(9);
    NimBLEDevice::setPower(9, NimBLETxPowerType::Advertise);

    NimBLEServer* server = NimBLEDevice::createServer();
    if (server == nullptr) {
        Serial.println("STATE-WAIT ble-server-failed");
        return;
    }
    server->advertiseOnDisconnect(true);
    NimBLEService* service = server->createService(scorebot::kGameServiceUuid);
    if (service == nullptr) {
        Serial.println("STATE-WAIT ble-service-failed");
        return;
    }

    NimBLECharacteristic* identity = service->createCharacteristic(
        scorebot::kIdentityUuid, NIMBLE_PROPERTY::READ, sizeof(peerId));
    NimBLECharacteristic* protocol = service->createCharacteristic(
        scorebot::kProtocolUuid, NIMBLE_PROPERTY::READ,
        sizeof(scorebot::kWireProtocolVersion));
    service->createCharacteristic(
        scorebot::kUplinkUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        scorebot::kMaxWireMessageSize);
    NimBLECharacteristic* downlink = service->createCharacteristic(
        scorebot::kDownlinkUuid,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        scorebot::kMaxWireMessageSize);
    if (identity == nullptr || protocol == nullptr || downlink == nullptr) {
        Serial.println("STATE-WAIT ble-characteristic-failed");
        return;
    }
    identity->setValue(peerId);
    protocol->setValue(scorebot::kWireProtocolVersion);
    downlink->setCallbacks(&downlinkCallbacks);

    // NimBLE 2 starts the GATT server from advertising. Service::start() is a no-op.
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    advertising->addServiceUUID(scorebot::kGameServiceUuid);
    advertising->setName(deviceName.c_str());
    advertising->setAdvertisingInterval(400);
    const bool started = advertising->start();
    Serial.printf(
        "STATE-WAIT id=%08lx advertising=%d\n",
        static_cast<unsigned long>(peerId), started);
}

void loop() {
    drainState();
    delay(10);
}
