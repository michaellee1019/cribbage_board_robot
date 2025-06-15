#ifndef MCP23017_HARDWARE_H
#define MCP23017_HARDWARE_H

#include <Adafruit_MCP23X17.h>
#include "../interfaces/IButtonHardware.hpp"

/**
 * @brief Concrete implementation of IButtonHardware using Adafruit MCP23X17
 */
class MCP23017Hardware : public IButtonHardware {
private:
    Adafruit_MCP23X17 mcp;
    TwoWire* wire;
    uint8_t address;

public:
    MCP23017Hardware(TwoWire* wire = &Wire, uint8_t address = 0x20) 
        : wire(wire), address(address) {}
    
    bool begin() override {
        return mcp.begin_I2C(address, wire);
    }
    
    void setupInterrupts(bool mirror, bool openDrain, uint8_t polarity) override {
        mcp.setupInterrupts(mirror, openDrain, polarity);
    }
    
    void setPinMode(uint8_t pin, uint8_t mode) override {
        mcp.pinMode(pin, mode);
    }
    
    void setupInterruptPin(uint8_t pin, uint8_t mode) override {
        mcp.setupInterruptPin(pin, mode);
    }
    
    uint8_t getLastInterruptPin() override {
        return mcp.getLastInterruptPin();
    }
    
    uint16_t getCapturedInterrupt() override {
        return mcp.getCapturedInterrupt();
    }
    
    void clearInterrupts() override {
        mcp.clearInterrupts();
    }
};

#endif // MCP23017_HARDWARE_H
