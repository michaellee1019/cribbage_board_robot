#include "Coordinator.hpp"

#include "Event.hpp"
#include "ErrorHandler.hpp"
#include <MyWifi.hpp>

[[noreturn]]
void dispatcherTask(void* param) {
    auto* coordinator = static_cast<Coordinator*>(param);
    Event e{};
    while (true) {
        if(xQueueReceive(coordinator->eventQueue, &e, portMAX_DELAY)) {
            coordinator->state.handleEvent(e, coordinator);
        }
    }
}


Coordinator::Coordinator() :
    eventQueue{xQueueCreate(10, sizeof(Event))},
    scheduler{},
    display{},
    buttonGrid{this},
    rotaryEncoder{this},
    wifi{this}
{
    CHECK_POINTER(eventQueue, ErrorCode::QUEUE_CREATE_FAILED, "Coordinator event queue");
}

void Coordinator::setup() {
    Serial.begin(115200);
    delay(2000);
    Wire.begin(5, 6);

    display.setup(0x70);
    wifi.setup();
    buttonGrid.setup();
    rotaryEncoder.setup();

    BaseType_t taskResult = xTaskCreate(dispatcherTask, "dispatcher", 4096, this, 2, nullptr);
    CHECK_FREERTOS_RESULT(taskResult, ErrorCode::TASK_CREATE_FAILED, "Coordinator dispatcher task");
}

void Coordinator::loop() {
    this->wifi.loop();
}

