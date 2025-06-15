#ifndef IBUTTON_HARDWARE_H
#define IBUTTON_HARDWARE_H

#include <cstdint>
#include <functional>

/**
 * @brief Interface for button hardware operations
 * 
 * This abstraction allows for mocking hardware in tests
 */
class IButtonHardware {
public:
    virtual ~IButtonHardware() = default;
    
    /**
     * @brief Initialize the hardware
     */
    virtual bool begin() = 0;
    
    /**
     * @brief Set up interrupts for the hardware
     */
    virtual void setupInterrupts(bool mirror, bool openDrain, uint8_t polarity) = 0;
    
    /**
     * @brief Set pin mode
     */
    virtual void setPinMode(uint8_t pin, uint8_t mode) = 0;
    
    /**
     * @brief Set up interrupt for a specific pin
     */
    virtual void setupInterruptPin(uint8_t pin, uint8_t mode) = 0;
    
    /**
     * @brief Get the last interrupt pin
     */
    virtual uint8_t getLastInterruptPin() = 0;
    
    /**
     * @brief Get captured interrupt
     */
    virtual uint16_t getCapturedInterrupt() = 0;
    
    /**
     * @brief Clear interrupts
     */
    virtual void clearInterrupts() = 0;
};

#endif // IBUTTON_HARDWARE_H
