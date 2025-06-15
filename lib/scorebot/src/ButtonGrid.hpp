#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include "interfaces/IButtonHardware.hpp"
#include "interfaces/IEventPublisher.hpp"

enum class Pins : uint32_t {
    Ok = 4,
    Add = 0,
    Interrupt = 8,
    PlusOne = 3,
    PlusFive = 2,
    MinusOne = 1,
};

static constexpr Pins AllPins[] = {
    Pins::Ok,
    Pins::Add,
    Pins::Interrupt,
    Pins::PlusOne,
    Pins::PlusFive,
    Pins::MinusOne
};

// https://stackoverflow.com/questions/111928/is-there-a-printf-converter-to-print-in-binary-format
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
((byte) & 0x80 ? '1' : '0'), \
((byte) & 0x40 ? '1' : '0'), \
((byte) & 0x20 ? '1' : '0'), \
((byte) & 0x10 ? '1' : '0'), \
((byte) & 0x08 ? '1' : '0'), \
((byte) & 0x04 ? '1' : '0'), \
((byte) & 0x02 ? '1' : '0'), \
((byte) & 0x01 ? '1' : '0')


struct ButtonStateHandle {
    const Pins& pin;
    const uint8_t& changedPins;
    const uint16_t& pinValues;

    [[nodiscard]]
    bool changed() const {
        // Ensure pin value is masked to fit within 8 bits (changedPins is uint8_t)
        auto pinBit = 1 << (static_cast<uint8_t>(pin) & 0x07);
        return (changedPins & pinBit) != 0;
    }

    [[nodiscard]]
    bool pressed() const {
        // Ensure pin value is masked to fit within 16 bits (pinValues is uint16_t)
        auto pinBit = 1 << (static_cast<uint8_t>(pin) & 0x0F);
        return (pinValues & pinBit) != 0;
    }

    [[nodiscard]]
    bool released() const {
        return !pressed();
    }
};

struct ButtonState {
    uint8_t changedPins;
    uint16_t pinValues;
    ButtonState(const uint8_t& changedPins, const uint16_t& pinValues)
        : changedPins{changedPins}, pinValues{pinValues}
    {}

    constexpr ButtonStateHandle operator[](Pins pin) const {
        return ButtonStateHandle{pin, changedPins, pinValues};
    }
};

class ButtonGrid {
private:
    IButtonHardware* hardware;
    IEventPublisher* eventPublisher;
    uint8_t interruptPin;

public:
    using Callback = std::function<void(const ButtonState& bs)>;

    void decodeInterrupt(const Callback& cb) {
        const auto changedPins = hardware->getLastInterruptPin();
        const auto pinValues = hardware->getCapturedInterrupt();

        struct ScopeGuard {
            IButtonHardware* hw;
            ~ScopeGuard() { hw->clearInterrupts(); }
        } sg{hardware};

        const ButtonState bs{changedPins, pinValues};
        cb(bs);
    }

    static uint32_t hardwarePin(Pins pin) {
        return static_cast<uint32_t>(pin);
    }

    // Expose hardware for testing
    IButtonHardware* getHardware() const { return hardware; }
    
    // Expose event publisher for testing
    IEventPublisher* getEventPublisher() const { return eventPublisher; }

    friend void buttonISR(void*);

public:
    /**
     * @brief Construct a new Button Grid object
     * 
     * @param hardware The hardware interface implementation
     * @param eventPublisher The event publisher implementation
     * @param interruptPin The pin used for interrupts
     */
    ButtonGrid(IButtonHardware* hardware, IEventPublisher* eventPublisher, uint8_t interruptPin = static_cast<uint8_t>(Pins::Interrupt));
    
    /**
     * @brief Legacy constructor for backward compatibility
     * 
     * @param coordinator Pointer to coordinator
     */
    explicit ButtonGrid(class Coordinator* coordinator);
    
    void setup();
};


#endif
