#include <RotaryEncoder.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>
#include <I2cBus.hpp>
#include <utils.hpp>

void IRAM_ATTR rotaryEncoderISR(void* arg) {
    const auto* self = static_cast<RotaryEncoder*>(arg);
    self->coordinator->enqueueInputFromISR(ButtonName::RotaryEncoder);
}

RotaryEncoder::RotaryEncoder(Coordinator *coordinator)
    : coordinator{coordinator},
      fade{*this}
{}

int32_t RotaryEncoder::position() {
    I2cBus::Guard guard;
    return ss.getEncoderPosition();
}

int32_t RotaryEncoder::delta() {
    I2cBus::Guard guard;
    return ss.getEncoderDelta();
}

void RotaryEncoder::setup() {
    I2cBus::Guard guard;
    if (!ss.begin(SEESAW_ADDR)) {
        FATAL_ERROR(ErrorCode::ENCODER_INIT_FAILED, "RotaryEncoder seesaw initialization failed");
    }
    if (!sspixel.begin(SEESAW_ADDR)) {
        FATAL_ERROR(ErrorCode::ENCODER_INIT_FAILED, "RotaryEncoder pixel initialization failed");
    }

    // https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
    ss.pinMode(SS_SWITCH, INPUT_PULLUP);

    static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

    pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
    ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
    ss.setGPIOInterrupts(mask, true);
    ss.enableEncoderInterrupt();

    attachInterruptArg(digitalPinToInterrupt(SEESAW_INTERRUPT), rotaryEncoderISR, this, FALLING);

    fade.setup();
    this->setColor(0x000000);
}

void RotaryEncoder::setColor(uint32_t color) {
    I2cBus::Guard guard;
    sspixel.setPixelColor(0, color);
    sspixel.show();
}

// TODO: setBrightness(0) doesn't work. Use setColor(0x000000) instead.
void RotaryEncoder::setBrightness(const uint8_t brightness) {
    I2cBus::Guard guard;
    sspixel.setBrightness(brightness);
    sspixel.setPixelColor(0, 0xFAEDED);
    if (brightness <= 0) {
        sspixel.clear();
    }
    sspixel.show();
}

void RotaryEncoder::lightOn() {
    fade.blinkEnabled();
}
void RotaryEncoder::lightOff() {
    fade.blinkDisabled();
}

void RotaryEncoder::reset() {
    I2cBus::Guard guard;
    ss.setEncoderPosition(0);
}

bool RotaryEncoder::pressed() {
    I2cBus::Guard guard;
    // Clear the GPIO interrupt flags on the seesaw chip
    static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;
    uint32_t data = ss.digitalReadBulk(mask);  // Reading clears the interrupt flags
    DEBUG_PRINTF("DEBUG: Rotary encoder pressed data: %d\n", data);
    return data == 0; // is 0 or 16777216. 0 is press down, 16777216 is everything else (press up and rotate)
}
