#ifndef COORDINATOR_H
#define COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <GameState.hpp>
#include <MyWifi.hpp>
#include <ButtonGrid.hpp>
#include <RotaryEncoder.hpp>
#include <HT16Display.hpp>
#include <BoardRole.hpp>

class Coordinator {
public:
    QueueHandle_t eventQueue;
    Scheduler scheduler;
    HT16Display display1;
    HT16Display display2;
    HT16Display display3;
    HT16Display display4;
    GameState state;
    ButtonGrid buttonGrid;
    RotaryEncoder rotaryEncoder;
    MyWifi wifi;

    Coordinator();
    void setup();
    void loop();
    BoardRole myRole();
    std::optional<BoardRoleConfig> myRoleConfig();

    friend void dispatcherTask(void*);
};

#endif // COORDINATOR_H