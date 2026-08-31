#include "Coordinator.hpp"

#include "Event.hpp"
#include "ErrorHandler.hpp"
#include <MyBle.hpp>
#include <I2cBus.hpp>
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
    eventQueue{xQueueCreate(32, sizeof(Event))},
    pendingInputEvents(0),
    display1{},
    display2{},
    display3{},
    display4{},
    buttonGrid{this},
    rotaryEncoder{this},
    ble{this}
{
    CHECK_POINTER(eventQueue, ErrorCode::QUEUE_CREATE_FAILED, "Coordinator event queue");
}

BoardRole Coordinator::myRole() {
    auto config = myRoleConfig();
    return config ? config->role : BoardRole::Unknown;
}

std::optional<BoardRoleConfig> Coordinator::myRoleConfig() {
    uint32_t myPeerId = ble.getMyPeerId();
    auto it = boardRoleConfig.find(myPeerId);
    return it != boardRoleConfig.end() ? std::make_optional(it->second) : std::nullopt;
}

void Coordinator::setup() {
    // Enable serial and wait for 5s delay to allow serial monitor to connect

    Serial.begin(115200);
    delay(5000);  // Ensure see the serial messages from the beginning.
    // The `while(serial)` thing doesn't terminate unless connected to a serial (USB).

    if (!Serial.available()) {
        Serial.setTimeout(1);
    }
    // may need an i2c lock because Wire.h almost certainly buffers.
    Wire.begin(5, 6);
    CHECK_FREERTOS_RESULT(I2cBus::initialize() ? pdPASS : pdFAIL,
                          ErrorCode::SEMAPHORE_CREATE_FAILED, "I2C mutex");
    // print i2c devices for debugging hardware
    printI2CDevices();

    // BLE identity is the device's factory MAC-derived id, so it must be initialized first.
    ble.setup();
    state.restore();
    if (myRole() == BoardRole::Leader) {
        state.leaderId = ble.getMyPeerId();
        state.leaderless = false;
        ++state.term;
        if (!state.persist()) {
            FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "leader startup state");
        }
    } else {
        // A saved replica is useful for recovery, but is never authority while
        // this board is waiting for a live leaderboard connection.
        state.leaderless = !ble.hasLeader();
    }

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
    state.refreshDisplays(this);
    updateDisplayBrightness();

    BaseType_t taskResult = xTaskCreate(dispatcherTask, "dispatcher", 4096, this, 2, nullptr);
    CHECK_FREERTOS_RESULT(taskResult, ErrorCode::TASK_CREATE_FAILED, "Coordinator dispatcher task");
}

void Coordinator::loop() {
    flushInputEvents();
    this->ble.loop();
    this->state.heartbeat(this);
    updateDisplayBrightness();
    delay(5);  // let the idle task run instead of continuously spinning a CPU core
}

void Coordinator::noteInteraction() {
    lastInteractionMs = millis();
}

void Coordinator::enqueueInputFromISR(ButtonName buttonName) {
    static_assert(std::atomic<uint8_t>::is_always_lock_free,
                  "input-event coalescing must remain ISR-safe");
    const uint8_t bit = buttonName == ButtonName::GPIOButtons ? 1u : 2u;
    pendingInputEvents.fetch_or(bit, std::memory_order_relaxed);
}

void Coordinator::flushInputEvents() {
    uint8_t pending = pendingInputEvents.exchange(0, std::memory_order_acq_rel);
    while (pending != 0) {
        const ButtonName buttonName = (pending & 1u) != 0
                                          ? ButtonName::GPIOButtons
                                          : ButtonName::RotaryEncoder;
        const uint8_t bit = buttonName == ButtonName::GPIOButtons ? 1u : 2u;
        Event event{};
        event.type = EventType::ButtonPressed;
        event.press.buttonName = buttonName;
        noteInteraction();
        if (xQueueSend(eventQueue, &event, 0) != pdPASS) {
            pendingInputEvents.fetch_or(pending, std::memory_order_release);
            return;
        }
        pending &= ~bit;
    }
}

void Coordinator::updateDisplayBrightness() {
    const uint32_t now = millis();
    const bool active = display_brightness::isInteractionActive(now, lastInteractionMs);
    if (active != displaysAreActive) {
        const uint8_t targetBrightness = display_brightness::targetFor(active);
        display1.setTargetBrightness(targetBrightness);
        display2.setTargetBrightness(targetBrightness);
        display3.setTargetBrightness(targetBrightness);
        display4.setTargetBrightness(targetBrightness);
        displaysAreActive = active;
    }

    display1.updateBrightness(now);
    display2.updateBrightness(now);
    display3.updateBrightness(now);
    display4.updateBrightness(now);
}
