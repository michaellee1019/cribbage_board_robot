#ifndef ADAFRUIT_MCP23X17_STUB_H
#define ADAFRUIT_MCP23X17_STUB_H

#include <cstdint>
#include "Arduino.h"

// Base class stub
class Adafruit_MCP23XXX {
public:
    virtual ~Adafruit_MCP23XXX() = default;
    virtual void clearInterrupts() {}
};

// MCP23X17 stub
class Adafruit_MCP23X17 : public Adafruit_MCP23XXX {
public:
    Adafruit_MCP23X17() {}
    
    bool begin_I2C(uint8_t addr = 0x20, TwoWire *wire = &Wire) {
        return true;
    }
    
    void pinMode(uint8_t pin, uint8_t mode) {}
    
    void digitalWrite(uint8_t pin, uint8_t value) {}
    
    uint8_t digitalRead(uint8_t pin) {
        return 0;
    }
    
    void setupInterrupts(bool mirroring, bool openDrain, uint8_t polarity) {}
    
    void setupInterruptPin(uint8_t pin, uint8_t mode) {}
    
    uint8_t getLastInterruptPin() {
        return 0;
    }
    
    uint16_t getCapturedInterrupt() {
        return 0;
    }
    
    void clearInterrupts() override {}
};

#endif // ADAFRUIT_MCP23X17_STUB_H
