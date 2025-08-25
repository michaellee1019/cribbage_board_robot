#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <Adafruit_MCP23X17.h>

class ButtonGrid {
    class Coordinator* coordinator;
public:
    Adafruit_MCP23X17 buttonGpio;
    static constexpr u32_t okPin = 4;
    static constexpr u32_t plusone = 3;
    static constexpr u32_t plusfive = 2;
    static constexpr u32_t negone = 1;
    static constexpr u32_t add = 0;

    static constexpr u32_t intValReleased = 31;
    static constexpr u32_t intValReleased2 = 63;
    static constexpr u32_t intValPressed = 17;

private:

    static constexpr u32_t interruptPin = 8;


    static constexpr auto pins = {okPin, plusone, plusfive, negone, add};

    friend void buttonISR(void*);

public:
    explicit ButtonGrid(class Coordinator* coordinator);
    void setup();
};


#endif