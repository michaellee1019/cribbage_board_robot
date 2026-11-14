#include <RotaryEncoder.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>
#include <I2cBus.hpp>
#include <PlayerBoard.hpp>
#include <utils.hpp>

#include <Adafruit_NeoPixel.h>
#include <soc/gpio_struct.h>

namespace {
portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
Adafruit_NeoPixel sideLeds(
    scorebot::player_pins::kNeoPixelCount,
    scorebot::player_pins::kNeoPixel,
    NEO_GRB + NEO_KHZ800);

constexpr int8_t kQuadrature[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

uint8_t encoderBitsFromLevel(uint32_t level) {
    const uint8_t a = static_cast<uint8_t>(
        (level >> scorebot::player_pins::kEncoderA) & 1u);
    const uint8_t b = static_cast<uint8_t>(
        (level >> scorebot::player_pins::kEncoderB) & 1u);
    return static_cast<uint8_t>((a << 1) | b);
}

uint8_t encoderBits() {
    return encoderBitsFromLevel(GPIO.in);
}
}  // namespace

void IRAM_ATTR rotaryEncoderISR(void* arg) {
    auto* self = static_cast<RotaryEncoder*>(arg);
    if (self->directInputs) {
        self->encoderState = static_cast<uint8_t>(
            (self->encoderState << 2) | encoderBitsFromLevel(GPIO.in));
        const int8_t movement = kQuadrature[self->encoderState & 0x0f];
        if (movement != 0) {
            portENTER_CRITICAL_ISR(&encoderMux);
            int8_t quarters = static_cast<int8_t>(self->quarterCount + movement);
            int32_t detents = self->detentCount;
            while (quarters >= 4) {
                ++detents;
                quarters = static_cast<int8_t>(quarters - 4);
            }
            while (quarters <= -4) {
                --detents;
                quarters = static_cast<int8_t>(quarters + 4);
            }
            self->quarterCount = quarters;
            self->detentCount = detents;
            portEXIT_CRITICAL_ISR(&encoderMux);
        }
    }
    self->coordinator->enqueueInputFromISR(ButtonName::RotaryEncoder);
}

RotaryEncoder::RotaryEncoder(Coordinator *coordinator)
    : coordinator{coordinator} {}

int32_t RotaryEncoder::delta() {
    if (directInputs) {
        portENTER_CRITICAL(&encoderMux);
        const int32_t value = detentCount;
        detentCount = 0;
        portEXIT_CRITICAL(&encoderMux);
        return value;
    }
    I2cBus::Guard guard;
    return ss.getEncoderDelta();
}

void RotaryEncoder::setup() {
    directInputs = scorebot::usesPlayerBoardHardware();
    if (directInputs) {
        pinMode(scorebot::player_pins::kEncoderA, INPUT_PULLUP);
        pinMode(scorebot::player_pins::kEncoderB, INPUT_PULLUP);
        pinMode(scorebot::player_pins::kEncoderSwitch, INPUT_PULLUP);
        encoderState = encoderBits();
        quarterCount = 0;
        detentCount = 0;
        sideLeds.begin();
        sideLeds.clear();
        sideLeds.show();
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderA),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderB),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderSwitch),
            rotaryEncoderISR, this, CHANGE);
        return;
    }

    {
        I2cBus::Guard guard;
        if (!ss.begin(SEESAW_ADDR)) {
            FATAL_ERROR(ErrorCode::ENCODER_INIT_FAILED, "RotaryEncoder seesaw initialization failed");
        }
        if (!sspixel.begin(SEESAW_ADDR)) {
            FATAL_ERROR(ErrorCode::ENCODER_INIT_FAILED, "RotaryEncoder pixel initialization failed");
        }

        // https://github.com/adafruit/Adafruit_seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
        ss.pinMode(SS_SWITCH, INPUT_PULLUP);

        static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

        pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
        ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
        ss.setGPIOInterrupts(mask, true);
        ss.enableEncoderInterrupt();

        attachInterruptArg(digitalPinToInterrupt(SEESAW_INTERRUPT), rotaryEncoderISR, this, FALLING);
    }

    // setColor takes the I2C mutex itself. It must run after the setup guard is
    // released or startup deadlocks before the dispatcher and main loop exist.
    this->setColor(0x000000);
}

void RotaryEncoder::setupStatusPixelOnly() {
    if (scorebot::usesPlayerBoardHardware()) {
        directInputs = true;
        sideLeds.begin();
        return;
    }
    I2cBus::Guard guard;
    // Do not reset the seesaw during a timer-only wake: its GPIO/encoder
    // interrupt configuration must survive so physical input can wake us from
    // the next deep-sleep interval.
    if (!sspixel.Adafruit_seesaw::begin(SEESAW_ADDR, -1, false)) {
        return;
    }
    sspixel.updateType(NEO_GRB + NEO_KHZ800);
    sspixel.updateLength(1);
    sspixel.setPin(SS_NEOPIX);
}

void RotaryEncoder::prepareForSleep() {
    if (directInputs) {
        detachInterrupt(digitalPinToInterrupt(scorebot::player_pins::kEncoderA));
        detachInterrupt(digitalPinToInterrupt(scorebot::player_pins::kEncoderB));
        detachInterrupt(digitalPinToInterrupt(scorebot::player_pins::kEncoderSwitch));
        return;
    }
    detachInterrupt(digitalPinToInterrupt(SEESAW_INTERRUPT));
    // Preserve any interrupt latched during the sleep handoff. Its active-low
    // output then causes an immediate EXT1 wake; normal setup clears/reset it.
}

void RotaryEncoder::resumeAfterSleepAbort() {
    if (directInputs) {
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderA),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderB),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderSwitch),
            rotaryEncoderISR, this, CHANGE);
        return;
    }
    attachInterruptArg(
        digitalPinToInterrupt(SEESAW_INTERRUPT), rotaryEncoderISR, this, FALLING);
}

bool RotaryEncoder::interruptAsserted() const {
    if (directInputs) {
        return digitalRead(scorebot::player_pins::kEncoderA) == LOW ||
               digitalRead(scorebot::player_pins::kEncoderB) == LOW ||
               digitalRead(scorebot::player_pins::kEncoderSwitch) == LOW;
    }
    return digitalRead(SEESAW_INTERRUPT) == LOW;
}

void RotaryEncoder::setColor(uint32_t color) {
    if (directInputs || scorebot::usesPlayerBoardHardware()) {
        for (int pixel = 0; pixel < scorebot::player_pins::kNeoPixelCount; ++pixel) {
            sideLeds.setPixelColor(pixel, color);
        }
        sideLeds.show();
        return;
    }
    I2cBus::Guard guard;
    sspixel.setPixelColor(0, color);
    sspixel.show();
}

void RotaryEncoder::reset() {
    if (directInputs) {
        portENTER_CRITICAL(&encoderMux);
        detentCount = 0;
        quarterCount = 0;
        portEXIT_CRITICAL(&encoderMux);
        return;
    }
    I2cBus::Guard guard;
    ss.setEncoderPosition(0);
}

bool RotaryEncoder::pressed() {
    if (directInputs) {
        return digitalRead(scorebot::player_pins::kEncoderSwitch) == LOW;
    }
    I2cBus::Guard guard;
    // Clear the GPIO interrupt flags on the seesaw chip
    static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;
    uint32_t data = ss.digitalReadBulk(mask);  // Reading clears the interrupt flags
    return data == 0; // is 0 or 16777216. 0 is press down, 16777216 is everything else (press up and rotate)
}
