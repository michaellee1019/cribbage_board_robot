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

// The measured Red/Yellow layout uses MCP pins 0..4 for ADD, -1, +1,
// +5, OK. Board 860931256 has +1/+5 on pins 3/2 instead. This follows
// hardware identity, so choosing another player color cannot change wiring.
constexpr uint8_t playerPlusOnePinForBoard(uint32_t node = 0) {
    return node == 860931256u ? 3 : 2;
}
constexpr uint8_t playerPlusFivePinForBoard(uint32_t node = 0) {
    return node == 860931256u ? 2 : 3;
}

// The four-button leaderboard is wired left-to-right on pins 1, 3, 0, 2, so
// pin 2 is its OK button and it has no +5 switch.
constexpr ButtonAction buttonActionForPin(bool leaderboard, uint8_t pin, uint32_t node = 0) {
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
    if (pin == playerPlusOnePinForBoard(node)) return ButtonAction::PlusOne;
    if (pin == playerPlusFivePinForBoard(node)) return ButtonAction::PlusFive;
    switch (pin) {
        case 0:
            return ButtonAction::Add;
        case 1:
            return ButtonAction::NegativeOne;
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
