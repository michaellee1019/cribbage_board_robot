#ifndef RotaryEncoder_h
#define RotaryEncoder_h

#include <HT16Display.hpp>
#include <GameState.hpp>

#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

class RotaryEncoder {
    static constexpr auto SS_SWITCH = 24;
    static constexpr auto SS_NEOPIX = 6;
    static constexpr auto SEESAW_ADDR = 0x36;
    static constexpr auto SEESAW_INTERRUPT = 7;
public:
    Coordinator* coordinator;
private:
    Adafruit_seesaw ss{};
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

public:

    explicit RotaryEncoder(Coordinator* coordinator);
    int32_t position();
    void setup();
    void lightOn();
    void lightOff();
};

#endif