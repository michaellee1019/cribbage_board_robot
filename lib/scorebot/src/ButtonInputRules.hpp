#pragma once

#include <cstdint>

namespace scorebot {

// The MCP23017 inputs use pull-ups, so the captured bit is low on press and
// high on release. Acting on the captured press works even when the release
// occurs before the latched interrupt has been serviced.
constexpr bool buttonCapturedPressed(uint8_t pin, uint16_t captured) {
    return pin < 16 && (captured & (static_cast<uint16_t>(1u) << pin)) == 0;
}

}  // namespace scorebot
