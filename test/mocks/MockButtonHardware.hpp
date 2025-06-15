#ifndef MOCK_BUTTON_HARDWARE_H
#define MOCK_BUTTON_HARDWARE_H

#include <map>
#include "../../lib/scorebot/src/interfaces/IButtonHardware.hpp"

/**
 * @brief Mock implementation of IButtonHardware for testing
 */
class MockButtonHardware : public IButtonHardware {
private:
    bool initialized = false;
    bool interruptsSetup = false;
    uint8_t lastInterruptPin = 0;
    uint16_t capturedInterrupt = 0;
    
    // For testing
    std::map<uint8_t, uint8_t> pinModes;
    std::map<uint8_t, uint8_t> interruptPins;

public:
    bool begin() override {
        initialized = true;
        return true;
    }
    
    void setupInterrupts(bool mirror, bool openDrain, uint8_t polarity) override {
        interruptsSetup = true;
    }
    
    void setPinMode(uint8_t pin, uint8_t mode) override {
        pinModes[pin] = mode;
    }
    
    void setupInterruptPin(uint8_t pin, uint8_t mode) override {
        interruptPins[pin] = mode;
    }
    
    uint8_t getLastInterruptPin() override {
        return lastInterruptPin;
    }
    
    uint16_t getCapturedInterrupt() override {
        return capturedInterrupt;
    }
    
    void clearInterrupts() override {
        // Do nothing in mock
    }
    
    // Test helper methods
    void simulateInterrupt(uint8_t pin, uint16_t value) {
        lastInterruptPin = pin;
        capturedInterrupt = value;
    }
    
    bool isInitialized() const {
        return initialized;
    }
    
    bool areInterruptsSetup() const {
        return interruptsSetup;
    }
    
    uint8_t getPinMode(uint8_t pin) const {
        auto it = pinModes.find(pin);
        return (it != pinModes.end()) ? it->second : 0;
    }
    
    uint8_t getInterruptPinMode(uint8_t pin) const {
        auto it = interruptPins.find(pin);
        return (it != interruptPins.end()) ? it->second : 0;
    }
};

#endif // MOCK_BUTTON_HARDWARE_H
