#ifndef RotaryEncoder_h
#define RotaryEncoder_h

#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

#include <HT16Display.hpp>
#include <callbacks.hpp>
#include <State.hpp>

inline std::map<int, String> playerNumberMap = {
    {1, "RED"},
    {2, "BLUE"},
    {3, "GREN"},
    {4, "WHIT"},
};


class RotaryEncoder {
#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SEESAW_ADDR 0x36
#define SEESAW_INTERRUPT 7

    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

    HT16Display* const display;

public:
    explicit RotaryEncoder(HT16Display* const display) : display{display} {}

    void setup() {
        ss.begin(SEESAW_ADDR);
        sspixel.begin(SEESAW_ADDR);
        sspixel.setBrightness(20);
        sspixel.setPixelColor(0, 0xFAEDED);
        sspixel.show();

        // https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
        ss.pinMode(SS_SWITCH, INPUT_PULLUP);

        static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

        pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
        ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
        ss.setGPIOInterrupts(mask, true);
        ss.enableEncoderInterrupt();

        attachInterrupt(digitalPinToInterrupt(SEESAW_INTERRUPT), seesawInterrupt, CHANGE);
    }

    void loop(State* const state) {
        if (!state->interrupted) {
            return;
        }
        auto pressed = !ss.digitalRead(SS_SWITCH);
        auto val = ss.getEncoderPosition();

        Serial.printf("playerNumber: %d\n", state->playerNumber);
        Serial.printf("playerColor: %s\n", playerNumberMap[state->playerNumber].c_str());
        Serial.printf("encoder val: %d\n", val);

        // initialize player selection
        if (!state->isLeaderboard && state->playerNumber == 0) {
            if (val > -1) {
                display->print(playerNumberMap[val]);
            }

            if (pressed) {
                state->playerNumber = val;
                Serial.printf("Player set to: %s\n", playerNumberMap[state->playerNumber].c_str());
            }
        }
        state->interrupted = false;
    }
};

#endif