#pragma once

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <memory>

namespace scorebot {
inline constexpr char kOtaServiceUuid[] = "c6a861b0-2f9d-46bc-9a23-bb9c89a519be";
}

class OtaControlCallbacks;
class OtaDataCallbacks;
void otaWorkerTask(void* parameter);

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
    void setTransportQuiescing(bool quiescing);
    void onDisconnected(uint16_t connectionHandle);
    void loop();

private:
    static constexpr uint16_t kChunkCapacity = 512;

    enum class RequestType : uint8_t {
        Control,
        Data,
        Disconnected,
    };

    struct Request {
        RequestType type;
        uint16_t connectionHandle;
        uint16_t length;
        std::array<uint8_t, kChunkCapacity> payload;
    };

    void enqueueControl(const NimBLEAttValue& value, uint16_t connectionHandle);
    void enqueueData(const NimBLEAttValue& value, uint16_t connectionHandle);
    void enqueueDisconnected(uint16_t connectionHandle);
    bool enqueueRequest(
        RequestType type, const uint8_t* data, size_t length,
        uint16_t connectionHandle);
    void processControl(const Request& request);
    void processData(Request& request);
    void processDisconnected(const Request& request);
    void workerLoop();
    void rejectConnection(uint16_t connectionHandle);
    void setStatus(
        const char* status,
        uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE);
    void abort(
        const char* status,
        uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE);

    NimBLECharacteristic* statusCharacteristic;
    NimBLEServer* server;
    SemaphoreHandle_t statusMutex;
    SemaphoreHandle_t admissionMutex;
    QueueHandle_t requestQueue;
    TaskHandle_t workerTask;
    std::atomic<bool> requestQueueFault;
    std::atomic<uint16_t> reservedConnectionHandle;
    std::array<uint16_t, 4> rejectedConnections;
    uint8_t rejectedConnectionCount;
    std::atomic<bool> transportQuiescing;
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
    friend void otaWorkerTask(void* parameter);
};
