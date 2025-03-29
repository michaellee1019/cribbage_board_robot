#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <HT16Display.hpp>
#include <callbacks.hpp>

#include <GameState.hpp>
#include <Adafruit_MCP23X17.h>

void IRAM_ATTR buttonISR(void* arg);

class ButtonGrid {
public:
    Coordinator* coordinator;
    HT16Display* const display;
    Adafruit_MCP23X17 buttonGpio;

    static constexpr u32_t interruptPin = 8;

    static constexpr u32_t okPin = 4;
    static constexpr u32_t plusone = 3;
    static constexpr u32_t plusfive = 2;
    static constexpr u32_t negone = 1;
    static constexpr u32_t add = 0;
    static constexpr auto pins = {okPin, plusone, plusfive, negone, add};

public:
    explicit ButtonGrid(Coordinator* coordinator,
                        HT16Display* const display) : coordinator{coordinator},
                                                      display{display} {}



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

    // Event getAndClearEvent() {
    //     uint8_t intPin = buttonGpio.getLastInterruptPin();   // Which pin caused it?
    //     uint8_t intVal = buttonGpio.getCapturedInterrupt();  // What was the level?
    //     buttonGpio.clearInterrupts();
    //     if (intPin == MCP23XXX_INT_ERR) {
    //         Serial.println("Button interrupt Error");
    //     }
    //
    //     Event out;
    //     out.type = EventType::ButtonPressed;
    //     // TODO: other things
    //     return out;
    // }
};


#endif