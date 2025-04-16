#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <cstdint>
#include <Adafruit_MCP23X17.h>

class ButtonGrid {
    class Coordinator* coordinator;
public:
    Adafruit_MCP23X17 buttonGpio;
    static constexpr uint32_t okPin = 4;
    static constexpr uint32_t add = 0;
private:

    static constexpr uint32_t interruptPin = 8;

    static constexpr uint32_t plusone = 3;
    static constexpr uint32_t plusfive = 2;
    static constexpr uint32_t negone = 1;

    static constexpr auto pins = {okPin, plusone, plusfive, negone, add};

    friend void buttonISR(void*);

public:
    explicit ButtonGrid(class Coordinator* coordinator);
    void setup();
};


#endif