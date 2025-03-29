#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <HT16Display.hpp>
#include <callbacks.hpp>

#include <GameState.hpp>
#include <Adafruit_MCP23X17.h>


//void IRAM_ATTR buttonISR(void* arg);
//
//void Buttons::setup(uint8_t buttonPin, uint8_t buttonId) {
//    pinMode(buttonPin, INPUT_PULLUP);
//    uint8_t* persistentButtonId = new uint8_t(buttonId);
//    attachInterruptArg(buttonPin, buttonISR, persistentButtonId, FALLING);
//}
//
//void IRAM_ATTR buttonISR(void* arg) {
//    uint8_t buttonId = *(uint8_t*)arg;
//    Event e{EventType::ButtonPressed, .buttonId=buttonId};
//    BaseType_t higherPriorityWoken = pdFALSE;
//    xQueueSendFromISR(eventQueue, &e, &higherPriorityWoken);
//    portYIELD_FROM_ISR(higherPriorityWoken);
//}


class ButtonGrid {
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
    explicit ButtonGrid(HT16Display* const display) : display{display} {}



    void setup() {
        buttonGpio.begin_I2C(0x20, &Wire);
        buttonGpio.setupInterrupts(true, false, LOW);
        for (auto&& pin : pins) {
            buttonGpio.pinMode(pin, INPUT_PULLUP);
            buttonGpio.setupInterruptPin(pin, CHANGE);
        }
        pinMode(interruptPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(interruptPin), buttonISR, CHANGE);
        buttonGpio.clearInterrupts();
    }

    void loop(GameState* volatile gameState) {
        if (!gameState->buttonPressed) {
            return;
        }

        uint8_t intPin = buttonGpio.getLastInterruptPin();   // Which pin caused it?
        uint8_t intVal = buttonGpio.getCapturedInterrupt();  // What was the level?
        if (intPin != MCP23XXX_INT_ERR) {
            display->print(strFormat("%d %2x", intPin, intVal));
        }
        gameState->buttonPressed = false;
        buttonGpio.clearInterrupts();
    }
};


#endif