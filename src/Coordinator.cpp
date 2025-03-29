#include "Coordinator.hpp"

#include "Event.hpp"
#include <MyWifi.hpp>

Coordinator::Coordinator() :
    eventQueue{xQueueCreate(10, sizeof(Event))},
    scheduler{},
    display{},
    buttonGrid{this,&display},
    wifi{this}
//    state(display),
//    buttons(eventQueue),
{}

void Coordinator::setup() {
    Serial.begin(115200);
    delay(2000);
    Wire.begin(5, 6);

//    buttons.setup(BUTTON_PIN, 1);
    display.setup(0x70);
    wifi.setup();
    buttonGrid.setup();

    xTaskCreate(dispatcherTask, "dispatcher", 4096, this, 2, nullptr);
}

void Coordinator::loop() {
    this->wifi.loop();
}

void Coordinator::dispatcherTask(void* param) {
    Coordinator* coordinator = static_cast<Coordinator*>(param);
    Event e;
    while (true) {
        if(xQueueReceive(coordinator->eventQueue, &e, portMAX_DELAY)) {
            coordinator->state.handleEvent(e, coordinator);
        }
    }
}