#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <PrinterRules.hpp>

#include <atomic>
#include <cstdint>

void printerWorkerTask(void* parameter);

// A leaderboard-only, demand-powered Wi-Fi/HTTP worker. Game code copies one
// immutable snapshot into its queue; all serialization and network waits stay
// off the dispatcher and state mutex.
class PrinterClient final {
public:
    PrinterClient();
    ~PrinterClient();
    PrinterClient(const PrinterClient&) = delete;
    PrinterClient& operator=(const PrinterClient&) = delete;

    bool setup(bool enabled);
    bool enqueue(const scorebot::PrinterSnapshot& snapshot);
    scorebot::PrinterProgress progress() const;
    scorebot::PrinterError error() const;
    uint32_t progressStartedMs() const;
    bool uiActive() const;
    bool busy() const;
    bool wifiPowered() const;
    bool dismissTerminal();

private:
    void workerLoop();
    void execute(const scorebot::PrinterSnapshot& snapshot);
    void publish(
        scorebot::PrinterProgress next,
        scorebot::PrinterError nextError = scorebot::PrinterError::None);
    bool powerWifiOff(bool eraseSavedAssociation);

    QueueHandle_t queue;
    TaskHandle_t workerTask;
    std::atomic<scorebot::PrinterProgress> currentProgress;
    std::atomic<scorebot::PrinterError> currentError;
    std::atomic<uint32_t> currentProgressStartedMs;
    std::atomic<bool> wifiIsPowered;
    bool enabled;

    friend void printerWorkerTask(void* parameter);
};
