#include <ButtonGrid.hpp>
#include <Coordinator.hpp>
#include <Event.hpp>

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
    buttonGpio.begin_I2C(0x20, &Wire);
    buttonGpio.setupInterrupts(true, false, LOW);
    for (auto&& pin : AllPins) {
        buttonGpio.pinMode(hardwarePin(pin), INPUT_PULLUP);
        buttonGpio.setupInterruptPin(hardwarePin(pin), CHANGE);
    }

    pinMode(hardwarePin(Pins::Interrupt), INPUT_PULLUP);
    attachInterruptArg(
        digitalPinToInterrupt(hardwarePin(Pins::Interrupt)),
        buttonISR,
        this,
        FALLING
    );
    buttonGpio.clearInterrupts();
}