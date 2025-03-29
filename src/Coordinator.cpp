#include "Coordinator.hpp"

#include "Event.hpp"
#include <MyWifi.hpp>

Coordinator::Coordinator() :
    eventQueue{xQueueCreate(10, sizeof(Event))},
    wifi{this}
//    display(),
//    state(display),
//    buttons(eventQueue),
//    wifi(eventQueue)
{}

void Coordinator::setup() {
//    buttons.setup(BUTTON_PIN, 1);
    wifi.setup();

    xTaskCreate(dispatcherTask, "dispatcher", 4096, this, 2, nullptr);
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