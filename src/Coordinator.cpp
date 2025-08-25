#include "Coordinator.hpp"

#include "Event.hpp"
#include "ErrorHandler.hpp"
#include <MyWifi.hpp>
#include <utils.hpp>

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
    display1{},
    display2{},
    display3{},
    display4{},
    buttonGrid{this},
    rotaryEncoder{this},
    wifi{this}
{
    CHECK_POINTER(eventQueue, ErrorCode::QUEUE_CREATE_FAILED, "Coordinator event queue");
}

BoardRole Coordinator::myRole() {
    auto config = myRoleConfig();
    return config ? config->role : BoardRole::Unknown;
}

std::optional<BoardRoleConfig> Coordinator::myRoleConfig() {
    uint32_t myPeerId = wifi.getMyPeerId();
    auto it = boardRoleConfig.find(myPeerId);
    return it != boardRoleConfig.end() ? std::make_optional(it->second) : std::nullopt;
}

void Coordinator::setup() {
    // Enable serial and wait for 5s delay to allow serial monitor to connect

    Serial.begin(115200);
    delay(5000);

    if (!Serial.available()) {
        Serial.setTimeout(1);
    }
    Wire.begin(5, 6);
    // print i2c devices for debugging hardware
    printI2CDevices();

    // Note: wifi.setup() must be called before myRole() is called.
    wifi.setup();

    display1.setup(0x70);
    display1.print("----");
    if (myRole() == BoardRole::Leader) {
        display2.setup(0x71);
        display2.print("----");
        display3.setup(0x72);
        display3.print("----");
        display4.setup(0x73);
        display4.print("----");
    }
    
    buttonGrid.setup();
    rotaryEncoder.setup();

    BaseType_t taskResult = xTaskCreate(dispatcherTask, "dispatcher", 4096, this, 2, nullptr);
    CHECK_FREERTOS_RESULT(taskResult, ErrorCode::TASK_CREATE_FAILED, "Coordinator dispatcher task");
}

void Coordinator::loop() {
    this->wifi.loop();
}

