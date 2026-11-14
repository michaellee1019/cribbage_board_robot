#include <ButtonGrid.hpp>
#include <Coordinator.hpp>
#include <Event.hpp>
#include <ErrorHandler.hpp>
#include <I2cBus.hpp>
#include <PlayerBoard.hpp>

#include <soc/gpio_struct.h>

namespace {
uint16_t samplePlayerButtons() {
    uint16_t pressed = 0;
    for (uint8_t index = 0; index < scorebot::player_pins::kButtonCount; ++index) {
        if (digitalRead(scorebot::player_pins::kButtonGpio[index]) == LOW) {
            pressed = static_cast<uint16_t>(pressed | (1u << index));
        }
    }
    return pressed;
}
}  // namespace

void IRAM_ATTR buttonISR(void* arg) {
    auto* self = static_cast<ButtonGrid*>(arg);
    if (self->directInputs) {
        // GPIO.in is register-backed, so this stays valid from an IRAM ISR.
        // GPIO 0-31 cover every player button.
        const uint32_t level = GPIO.in;
        uint16_t pressed = 0;
        for (uint8_t index = 0; index < scorebot::player_pins::kButtonCount; ++index) {
            const int gpio = scorebot::player_pins::kButtonGpio[index];
            if (((level >> gpio) & 1u) == 0u) {
                pressed = static_cast<uint16_t>(pressed | (1u << index));
            }
        }
        const uint16_t changed =
            static_cast<uint16_t>(pressed ^ self->directPressed);
        uint8_t pin = self->directPin;
        for (uint8_t index = 0; index < scorebot::player_pins::kButtonCount; ++index) {
            if ((changed & (1u << index)) != 0) {
                pin = index;
                break;
            }
        }
        self->directPressed = pressed;
        self->directPin = pin;
        // Match the MCP capture: a pressed input is a zero bit.
        self->directCaptured = static_cast<uint16_t>(~pressed);
    }
    self->coordinator->enqueueInputFromISR(ButtonName::GPIOButtons);
}

ButtonGrid::ButtonGrid(Coordinator* coordinator) : coordinator{coordinator}{}

void ButtonGrid::setup() {
    directInputs = scorebot::usesPlayerBoardHardware();
    if (directInputs) {
        directPressed = samplePlayerButtons();
        directCaptured = static_cast<uint16_t>(~directPressed);
        for (uint8_t index = 0; index < scorebot::player_pins::kButtonCount; ++index) {
            const int gpio = scorebot::player_pins::kButtonGpio[index];
            pinMode(gpio, INPUT_PULLUP);
            attachInterruptArg(digitalPinToInterrupt(gpio), buttonISR, this, CHANGE);
        }
        return;
    }

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
    if (directInputs) {
        return Interrupt{directPin, directCaptured, samplePlayerButtons()};
    }

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
    if (directInputs) {
        return samplePlayerButtons();
    }

    I2cBus::Guard guard;
    constexpr uint16_t buttonMask =
        (1u << okPin) | (1u << plusone) | (1u << plusfive) |
        (1u << negone) | (1u << add);
    return static_cast<uint16_t>(~buttonGpio.readGPIOAB()) & buttonMask;
}

void ButtonGrid::prepareForSleep() {
    if (directInputs) {
        for (uint8_t index = 0; index < scorebot::player_pins::kButtonCount; ++index) {
            detachInterrupt(digitalPinToInterrupt(
                scorebot::player_pins::kButtonGpio[index]));
        }
        return;
    }
    detachInterrupt(digitalPinToInterrupt(interruptPin));
    // Do not clear the peripheral latch here. An input arriving during the
    // sleep handoff must keep the active-low line asserted so EXT1 wakes the
    // board immediately instead of losing that interaction.
}

void ButtonGrid::resumeAfterSleepAbort() {
    if (directInputs) {
        for (uint8_t index = 0; index < scorebot::player_pins::kButtonCount; ++index) {
            attachInterruptArg(
                digitalPinToInterrupt(scorebot::player_pins::kButtonGpio[index]),
                buttonISR, this, CHANGE);
        }
        return;
    }
    attachInterruptArg(
        digitalPinToInterrupt(interruptPin), buttonISR, this, FALLING);
}

bool ButtonGrid::interruptAsserted() const {
    if (directInputs) {
        return samplePlayerButtons() != 0;
    }
    return digitalRead(interruptPin) == LOW;
}
