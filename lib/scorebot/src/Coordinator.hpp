#ifndef COORDINATOR_H
#define COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <GameState.hpp>
#include <MyWifi.hpp>
//#include "Buttons.h"
//#include "Wifi.h"
//#include "Display.h"

class Coordinator {
public:
    Coordinator();
    void setup();

    QueueHandle_t eventQueue;
//    Display display;
    GameState state;
//    Buttons buttons;
    MyWifi wifi;

    static void dispatcherTask(void* param);
};

#endif // COORDINATOR_H