#include <RotaryEncoder.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>
#include <I2cBus.hpp>
#include <PlayerBoard.hpp>
#include <utils.hpp>

#include <Adafruit_NeoPixel.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>
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

inline uint32_t IRAM_ATTR cpuCycleCount() {
    uint32_t cycles;
    asm volatile("rsr %0, ccount" : "=a"(cycles));
    return cycles;
}

// SK6812 / WS2812 bit times at 240 MHz. GPIO48 is outside the low GPIO
// register, and the ESP32-S3 RMT channel is too small to hold eight pixels,
// so the strip is shifted out directly.
constexpr uint32_t kLedPinMask = 1u << (scorebot::player_pins::kNeoPixel - 32);
constexpr uint32_t kZeroHighCycles = 70;    // 0.29 us
constexpr uint32_t kZeroPeriodCycles = 300; // 1.25 us
constexpr uint32_t kOneHighCycles = 168;    // 0.70 us
constexpr uint32_t kOnePeriodCycles = 300;

void IRAM_ATTR showSideLeds(const uint8_t* data, int nbytes) {
    portDISABLE_INTERRUPTS();
    for (int byteIndex = 0; byteIndex < nbytes; ++byteIndex) {
        uint8_t value = data[byteIndex];
        for (int bit = 0; bit < 8; ++bit) {
            const bool one = (value & 0x80u) != 0;
            const uint32_t highCycles = one ? kOneHighCycles : kZeroHighCycles;
            const uint32_t periodCycles = one ? kOnePeriodCycles : kZeroPeriodCycles;
            const uint32_t start = cpuCycleCount();
            GPIO.out1_w1ts.val = kLedPinMask;
            while ((cpuCycleCount() - start) < highCycles) {
            }
            GPIO.out1_w1tc.val = kLedPinMask;
            while ((cpuCycleCount() - start) < periodCycles) {
            }
            value = static_cast<uint8_t>(value << 1);
        }
    }
    GPIO.out1_w1tc.val = kLedPinMask;
    portENABLE_INTERRUPTS();
}

void paintSideLeds(uint32_t color) {
    for (int pixel = 0; pixel < scorebot::player_pins::kNeoPixelCount; ++pixel) {
        sideLeds.setPixelColor(pixel, color);
    }
    pinMode(scorebot::player_pins::kNeoPixel, OUTPUT);
    digitalWrite(scorebot::player_pins::kNeoPixel, LOW);
    ets_delay_us(80);
    showSideLeds(sideLeds.getPixels(), sideLeds.numPixels() * 3);
}
}  // namespace

void IRAM_ATTR rotaryEncoderISR(void* arg) {
    auto* self = static_cast<RotaryEncoder*>(arg);
    if (self->directInputs) {
        const uint8_t previous = static_cast<uint8_t>(self->encoderState & 0x03);
        const uint8_t next = encoderBitsFromLevel(GPIO.in);
        self->encoderState = static_cast<uint8_t>((self->encoderState << 2) | next);
        int8_t movement = kQuadrature[self->encoderState & 0x0f];
        // A missed intermediate edge changes both contacts at once. Keep the
        // current direction instead of dropping that part of the detent.
        if (movement == 0 && next != previous && self->lastDirection != 0) {
            movement = static_cast<int8_t>(self->lastDirection * 2);
        }
        const int64_t now = esp_timer_get_time();
        portENTER_CRITICAL_ISR(&encoderMux);
        if (movement != 0) {
            const int8_t direction = movement > 0 ? 1 : -1;
            // Contact bounce is a fast reversal. Ignore it, but keep a real
            // click that simply has uneven edges.
            if (!(direction == -self->lastDirection &&
                  (now - self->lastTransitionUs) < 500)) {
                self->quarterCount =
                    static_cast<int8_t>(self->quarterCount + movement);
                self->lastDirection = direction;
            }
            self->lastTransitionUs = now;
        }
        if (previous == 0x03 && next != 0x03) {
            self->leftRestUs = now;
        }
        // The PEC11R rests with both contacts open. Count one tick per click
        // once it returns there, even if one of the four edges was missed.
        if (next == 0x03 && previous != 0x03 && self->quarterCount != 0) {
            const int8_t quarters = self->quarterCount;
            const bool realClick = (now - self->leftRestUs) >= 1200 ||
                                   quarters >= 2 || quarters <= -2;
            if (realClick) {
                self->detentCount += quarters > 0 ? 1 : -1;
            }
            self->quarterCount = 0;
        }
        portEXIT_CRITICAL_ISR(&encoderMux);
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
        lastDirection = 0;
        sideLeds.begin();
        paintSideLeds(0);
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
        paintSideLeds(color);
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
