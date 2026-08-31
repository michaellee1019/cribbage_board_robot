#include <ButtonGrid.hpp>
#include <Coordinator.hpp>
#include <Event.hpp>
#include <ErrorHandler.hpp>
#include <I2cBus.hpp>

void IRAM_ATTR buttonISR(void* arg) {
    const auto* self = static_cast<ButtonGrid*>(arg);
    self->coordinator->enqueueInputFromISR(ButtonName::GPIOButtons);
}

ButtonGrid::ButtonGrid(Coordinator* coordinator) : coordinator{coordinator}{}

void ButtonGrid::setup() {
    I2cBus::Guard guard;
    if (!buttonGpio.begin_I2C(0x20, &Wire)) {
        FATAL_ERROR(ErrorCode::I2C_INIT_FAILED, "ButtonGrid I2C initialization failed");
    }
    buttonGpio.setupInterrupts(true, false, LOW);
    for (auto&& pin : pins) {
        buttonGpio.pinMode(pin, INPUT_PULLUP);
        buttonGpio.setupInterruptPin(pin, CHANGE);
    }
    pinMode(interruptPin, INPUT_PULLUP);
    // MCP23x17 is configured active-low; reading INTCAP clears it and creates
    // a rising edge, so react only to the asserted edge.
    attachInterruptArg(digitalPinToInterrupt(interruptPin), buttonISR, this, FALLING);
    buttonGpio.clearInterrupts();
}

ButtonGrid::Interrupt ButtonGrid::consumeInterrupt() {
    I2cBus::Guard guard;
    const Interrupt result{buttonGpio.getLastInterruptPin(), buttonGpio.getCapturedInterrupt()};
    buttonGpio.clearInterrupts();
    return result;
}
