#ifndef COORDINATOR_H
#define COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

#include <GameState.hpp>
#include <DisplayBrightness.hpp>
#include <MyBle.hpp>
#include <ButtonGrid.hpp>
#include <RotaryEncoder.hpp>
#include <HT16Display.hpp>
#include <BoardRole.hpp>

class Coordinator {
public:
    QueueHandle_t eventQueue;
    std::atomic<uint8_t> pendingInputEvents;
    HT16Display display1;
    HT16Display display2;
    HT16Display display3;
    HT16Display display4;
    GameState state;
    ButtonGrid buttonGrid;
    RotaryEncoder rotaryEncoder;
    MyBle ble;

    Coordinator();
    void setup();
    void loop();
    void enqueueInputFromISR(ButtonName buttonName);
    void noteInteraction();
    void setPlayerTurnAnimation(bool enabled);
    void setLeaderboardTurnColor(uint32_t color);
    BoardRole myRole();
    std::optional<BoardRoleConfig> myRoleConfig();

    friend void dispatcherTask(void*);

private:
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

    void flushInputEvents();
    void updateDisplayBrightness();
};

#endif // COORDINATOR_H
