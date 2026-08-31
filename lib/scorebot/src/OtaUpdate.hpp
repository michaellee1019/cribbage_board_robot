#pragma once

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace scorebot {
inline constexpr char kOtaServiceUuid[] = "c6a861b0-2f9d-46bc-9a23-bb9c89a519be";
}

class OtaControlCallbacks;
class OtaDataCallbacks;

// A locally armed BLE firmware writer. Player boards advertise the service
// while waiting for their leaderboard; the leaderboard advertises only during
// the physical-presence window opened by the rotary-button gesture.
class OtaUpdate {
public:
    OtaUpdate();
    ~OtaUpdate();
    OtaUpdate(const OtaUpdate&) = delete;
    OtaUpdate& operator=(const OtaUpdate&) = delete;

    void setup(NimBLEServer* server);
    void arm();
    bool isArmed() const;
    bool isActive() const;
    bool isWriting() const;
    void onDisconnected(uint16_t connectionHandle);
    void loop();

private:
    void handleControl(const NimBLEAttValue& value, uint16_t connectionHandle);
    void handleData(const NimBLEAttValue& value, uint16_t connectionHandle);
    void setStatus(const char* status);
    void abort(const char* status);

    NimBLECharacteristic* statusCharacteristic;
    NimBLEServer* server;
    SemaphoreHandle_t statusMutex;
    std::atomic<uint32_t> armUntilMs;
    uint32_t expectedBytes;
    uint32_t receivedBytes;
    std::atomic<uint16_t> writerConnectionHandle;
    std::atomic<uint32_t> lastProgressMs;
    std::atomic<bool> timeoutDisconnectRequested;
    std::atomic<uint32_t> restartAtMs;
    std::atomic<bool> armed;
    std::atomic<bool> writing;
    std::unique_ptr<OtaControlCallbacks> controlCallbacks;
    std::unique_ptr<OtaDataCallbacks> dataCallbacks;

    friend class OtaControlCallbacks;
    friend class OtaDataCallbacks;
};
