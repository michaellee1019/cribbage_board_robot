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
    const uint8_t pin = buttonGpio.getLastInterruptPin();
    const uint16_t captured = buttonGpio.getCapturedInterrupt();
    const uint16_t gpio = buttonGpio.readGPIOAB();
    constexpr uint16_t buttonMask =
        (1u << okPin) | (1u << plusone) | (1u << plusfive) |
        (1u << negone) | (1u << add);
    const Interrupt result{
        pin,
        captured,
        static_cast<uint16_t>(static_cast<uint16_t>(~gpio) & buttonMask),
    };
    buttonGpio.clearInterrupts();
    return result;
}

uint16_t ButtonGrid::pressedMask() {
    I2cBus::Guard guard;
    constexpr uint16_t buttonMask =
        (1u << okPin) | (1u << plusone) | (1u << plusfive) |
        (1u << negone) | (1u << add);
    return static_cast<uint16_t>(~buttonGpio.readGPIOAB()) & buttonMask;
}

void ButtonGrid::prepareForSleep() {
    detachInterrupt(digitalPinToInterrupt(interruptPin));
    // Do not clear the peripheral latch here. An input arriving during the
    // sleep handoff must keep the active-low line asserted so EXT1 wakes the
    // board immediately instead of losing that interaction.
}

void ButtonGrid::resumeAfterSleepAbort() {
    attachInterruptArg(
        digitalPinToInterrupt(interruptPin), buttonISR, this, FALLING);
}

bool ButtonGrid::interruptAsserted() const {
    return digitalRead(interruptPin) == LOW;
}
