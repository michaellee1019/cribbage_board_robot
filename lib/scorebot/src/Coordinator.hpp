#ifndef COORDINATOR_H
#define COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <GameState.hpp>
#include <MyWifi.hpp>
#include <ButtonGrid.hpp>
//#include "Buttons.h"
//#include "Wifi.h"
//#include "Display.h"
#include <HT16Display.hpp>

class Coordinator {
public:
    QueueHandle_t eventQueue;
    Scheduler scheduler;
    HT16Display display;
    GameState state;
    ButtonGrid buttonGrid;
    //    Buttons buttons;
    MyWifi wifi;
public:
    Coordinator();
    void setup();
    void loop();
    static void dispatcherTask(void* param);
};

#endif // COORDINATOR_H