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
    BoardRole myRole();
    std::optional<BoardRoleConfig> myRoleConfig();

    friend void dispatcherTask(void*);

private:
    uint32_t lastInteractionMs{0};
    // Start true so the first update applies the idle target after display setup.
    bool displaysAreActive{true};

    void flushInputEvents();
    void updateDisplayBrightness();
};

#endif // COORDINATOR_H
