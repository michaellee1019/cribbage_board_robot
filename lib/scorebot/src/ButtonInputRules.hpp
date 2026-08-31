#pragma once

#include <cstdint>

namespace scorebot {

enum class ButtonAction : uint8_t {
    None,
    Add,
    NegativeOne,
    PlusFive,
    PlusOne,
    Ok,
};

// Player boards expose five switches on MCP pins 0..4. The four-button
// leaderboard is physically wired left-to-right on pins 1, 3, 0, 2, so pin 2
// is its OK button and it has no +5 switch.
constexpr ButtonAction buttonActionForPin(bool leaderboard, uint8_t pin) {
    if (leaderboard) {
        switch (pin) {
            case 0:
                return ButtonAction::PlusOne;
            case 1:
                return ButtonAction::Add;
            case 2:
                return ButtonAction::Ok;
            case 3:
                return ButtonAction::NegativeOne;
            default:
                return ButtonAction::None;
        }
    }
    switch (pin) {
        case 0:
            return ButtonAction::Add;
        case 1:
            return ButtonAction::NegativeOne;
        case 2:
            return ButtonAction::PlusFive;
        case 3:
            return ButtonAction::PlusOne;
        case 4:
            return ButtonAction::Ok;
        default:
            return ButtonAction::None;
    }
}

// The MCP23017 inputs use pull-ups, so the captured bit is low on press and
// high on release. Acting on the captured press works even when the release
// occurs before the latched interrupt has been serviced.
constexpr bool buttonCapturedPressed(uint8_t pin, uint16_t captured) {
    return pin < 16 && (captured & (static_cast<uint16_t>(1u) << pin)) == 0;
}

}  // namespace scorebot
