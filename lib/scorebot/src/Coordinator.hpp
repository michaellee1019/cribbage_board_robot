#ifndef COORDINATOR_H
#define COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>

#include <GameState.hpp>
#include <DisplayBrightness.hpp>
#include <MyBle.hpp>
#include <ButtonGrid.hpp>
#include <RotaryEncoder.hpp>
#include <HT16Display.hpp>
#include <BoardRole.hpp>
#include <UsbConnection.hpp>

class Coordinator {
private:
    QueueHandle_t eventQueue;
    SemaphoreHandle_t stateMutex;
    GameState state;

public:
    std::atomic<uint8_t> pendingInputEvents;
    HT16Display display1;
    HT16Display display2;
    HT16Display display3;
    HT16Display display4;
    ButtonGrid buttonGrid;
    RotaryEncoder rotaryEncoder;
    MyBle ble;
    UsbConnectionMonitor usbConnection;

    Coordinator();
    void setup();
    void loop();
    void enqueueInputFromISR(ButtonName buttonName);
    void noteInteraction();
    void armOta();
    bool otaModeActive() const;
    void setPlayerTurnAnimation(bool enabled);
    void setLeaderboardTurnColor(uint32_t color);
    BoardRole myRole();
    const BoardRoleConfig& myRoleConfig();
    void serviceStateHeartbeat();
    bool enqueueEvent(const Event& event);

    friend void dispatcherTask(void*);

private:
    uint32_t awakeSinceMs{0};
    std::atomic<uint32_t> lastInteractionMs{0};
    std::atomic<bool> playerTurnAnimationActive{false};
    std::atomic<uint32_t> playerTurnAnimationStartedMs{0};
    std::atomic<uint32_t> leaderboardTurnColor{0};
    uint8_t lastTurnSegmentBrightness{0xff};
    uint32_t lastTurnLightColor{0xffffffff};
    uint32_t lastLeaderboardLightColor{0xffffffff};
    bool turnAnimationWasActive{false};
    // Start true so the first update applies the idle target after display setup.
    bool displaysAreActive{true};
    std::atomic<bool> sleeping{false};
    std::atomic<bool> otaUiActive{false};
    uint32_t lastOtaLightColor{0xffffffff};

    void flushInputEvents();
    void updateDisplayBrightness();
    void showOtaUi();
    void finishOtaUi();
    bool sleepBlocked() const;
};

#endif // COORDINATOR_H
