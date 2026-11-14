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
    bool directInputs{false};
    volatile int32_t detentCount{0};
    volatile int8_t quarterCount{0};
    volatile uint8_t encoderState{0};
public:

    explicit RotaryEncoder(Coordinator* coordinator);
    friend void rotaryEncoderISR(void* arg);
    /** Clears the interrupt flag. */
    int32_t delta();
    void setup();
    void setupStatusPixelOnly();
    void prepareForSleep();
    void resumeAfterSleepAbort();
    bool interruptAsserted() const;
    void setColor(uint32_t color);
    void reset();
    bool pressed();
};

#endif
