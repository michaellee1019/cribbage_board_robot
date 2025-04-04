#include <RotaryEncoder.hpp>
#include <Coordinator.hpp>

void IRAM_ATTR rotaryEncoderISR(void* arg) {
    const auto* self = static_cast<RotaryEncoder*>(arg);
    Event event{};
    event.type = EventType::ButtonPressed;
    BaseType_t higherPriorityWoken = pdFALSE;
    xQueueSendFromISR(self->coordinator->eventQueue, &event, &higherPriorityWoken);
    portYIELD_FROM_ISR(higherPriorityWoken);
}

RotaryEncoder::RotaryEncoder(Coordinator *coordinator)
    : coordinator{coordinator},
      fade{*this}
{}

int32_t RotaryEncoder::position() {
    return ss.getEncoderPosition();
}

void RotaryEncoder::setup() {
    ss.begin(SEESAW_ADDR);
    sspixel.begin(SEESAW_ADDR);

    // https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
    ss.pinMode(SS_SWITCH, INPUT_PULLUP);

    static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

    pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
    ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
    ss.setGPIOInterrupts(mask, true);
    ss.enableEncoderInterrupt();

    attachInterruptArg(digitalPinToInterrupt(SEESAW_INTERRUPT), rotaryEncoderISR, this, CHANGE);

    fade.setup();
}

void RotaryEncoder::setBrightness(const uint8_t brightness) {
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
