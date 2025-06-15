#include <ButtonGrid.hpp>
#include <Coordinator.hpp>
#include <Event.hpp>
#include <ErrorHandler.hpp>

void IRAM_ATTR buttonISR(void* arg) {
    const auto* self = static_cast<ButtonGrid*>(arg);
    Event event{};
    event.type = EventType::ButtonPressed;
    BaseType_t higherPriorityWoken = pdFALSE;
    xQueueSendFromISR(self->coordinator->eventQueue, &event, &higherPriorityWoken);
    portYIELD_FROM_ISR(higherPriorityWoken);
}

ButtonGrid::ButtonGrid(Coordinator* coordinator) : coordinator{coordinator}{}

void ButtonGrid::setup() {
    if (!buttonGpio.begin_I2C(0x20, &Wire)) {
        FATAL_ERROR(ErrorCode::I2C_INIT_FAILED, "ButtonGrid I2C initialization failed");
    }
    buttonGpio.setupInterrupts(true, false, LOW);
    for (auto&& pin : pins) {
        buttonGpio.pinMode(pin, INPUT_PULLUP);
        buttonGpio.setupInterruptPin(pin, CHANGE);
    }
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(interruptPin), buttonISR, this, CHANGE);
    buttonGpio.clearInterrupts();
}