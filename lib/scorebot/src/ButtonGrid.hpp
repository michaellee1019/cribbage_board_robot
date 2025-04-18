#ifndef BUTTONGRID_H
#define BUTTONGRID_H

#include <Adafruit_MCP23X17.h>

enum class Pins : u32_t {
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
    const uint16_t& changedPins;
    const uint16_t& pinValues;

    [[nodiscard]]
    bool changed() const {
        return changedPins & (1 << static_cast<uint8_t>(pin));
    }

    [[nodiscard]]
    bool pressed() const {
        return pinValues & (1 << static_cast<uint8_t>(pin));
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
    class Coordinator* coordinator;
public:
    Adafruit_MCP23X17 buttonGpio;

    using Callback = std::function<void(const ButtonState& bs)>;

    void decodeInterrupt(const Callback& cb) {
        const auto changedPins = buttonGpio.getLastInterruptPin();
        const auto pinValues = buttonGpio.getCapturedInterrupt();

        struct ScopeGuard {
            Adafruit_MCP23XXX& gpio;
            ~ScopeGuard() { gpio.clearInterrupts(); }
        } sg{buttonGpio};

        const ButtonState bs{changedPins, pinValues};
        cb(bs);
    }

    static u32_t hardwarePin(Pins pin) {
        return static_cast<u32_t>(pin);
    }


    friend void buttonISR(void*);

public:
    explicit ButtonGrid(class Coordinator* coordinator);
    void setup();
};


#endif