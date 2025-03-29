#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <GameState.hpp>

#include <Adafruit_MCP23X17.h>

void IRAM_ATTR buttonISR(void* arg);


class ButtonGrid {
public:
    Coordinator* coordinator;
    Adafruit_MCP23X17 buttonGpio;

    static constexpr u32_t interruptPin = 8;

    static constexpr u32_t okPin = 4;
    static constexpr u32_t plusone = 3;
    static constexpr u32_t plusfive = 2;
    static constexpr u32_t negone = 1;
    static constexpr u32_t add = 0;
    static constexpr auto pins = {okPin, plusone, plusfive, negone, add};

    explicit ButtonGrid(Coordinator* coordinator) : coordinator{coordinator}{}

    void setup() {
        buttonGpio.begin_I2C(0x20, &Wire);
        buttonGpio.setupInterrupts(true, false, LOW);
        for (auto&& pin : pins) {
            buttonGpio.pinMode(pin, INPUT_PULLUP);
            buttonGpio.setupInterruptPin(pin, CHANGE);
        }
        pinMode(interruptPin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(interruptPin), buttonISR, this, CHANGE);
        buttonGpio.clearInterrupts();
    }
};


#endif