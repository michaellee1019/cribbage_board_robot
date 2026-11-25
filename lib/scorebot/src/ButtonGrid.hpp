#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <Adafruit_MCP23X17.h>
#include <atomic>

class ButtonGrid {
    class Coordinator* coordinator;
public:
    struct Interrupt {
        uint8_t pin;
        uint16_t captured;
        uint16_t pressed;
        uint16_t capturedPresses{0};
        bool capturedOverlap{false};
        uint32_t firstEdgeUs{0};
    };
    Adafruit_MCP23X17 buttonGpio;
    static constexpr u32_t okPin = 4;
    static constexpr u32_t pin2 = 2;
    static constexpr u32_t pin3 = 3;
    static constexpr u32_t negone = 1;
    static constexpr u32_t add = 0;

private:

    static constexpr u32_t interruptPin = 8;


    static constexpr auto pins = {okPin, pin2, pin3, negone, add};
    bool directInputs{false};
    uint16_t directPressed{0};
    std::atomic<uint32_t> directCaptured{0};
    std::atomic<uint32_t> firstEdgeUs{0};

    friend void buttonISR(void*);

public:
    explicit ButtonGrid(class Coordinator* coordinator);
    void setup();
    void prepareForSleep();
    void resumeAfterSleepAbort();
    bool interruptAsserted() const;
    Interrupt consumeInterrupt();
    uint16_t pressedMask();
};


#endif
