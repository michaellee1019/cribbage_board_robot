#pragma once

#include <NimBLEDevice.h>

#include <atomic>
#include <cstdint>
#include <memory>

class OtaControlCallbacks;
class OtaDataCallbacks;

// A locally armed BLE firmware writer. The service is always discoverable, but
// it accepts START only during the short physical-presence window opened by the
// rotary-button gesture.
class OtaUpdate {
public:
    OtaUpdate();
    ~OtaUpdate();
    OtaUpdate(const OtaUpdate&) = delete;
    OtaUpdate& operator=(const OtaUpdate&) = delete;

    void setup(NimBLEServer* server);
    void arm();
    bool isArmed() const;
    void loop();

private:
    void handleControl(const NimBLEAttValue& value);
    void handleData(const NimBLEAttValue& value);
    void setStatus(const char* status);
    void abort(const char* status);

    NimBLECharacteristic* statusCharacteristic;
    std::atomic<uint32_t> armUntilMs;
    uint32_t expectedBytes;
    uint32_t receivedBytes;
    std::atomic<uint32_t> restartAtMs;
    std::atomic<bool> armed;
    std::atomic<bool> writing;
    std::unique_ptr<OtaControlCallbacks> controlCallbacks;
    std::unique_ptr<OtaDataCallbacks> dataCallbacks;

    friend class OtaControlCallbacks;
    friend class OtaDataCallbacks;
};
