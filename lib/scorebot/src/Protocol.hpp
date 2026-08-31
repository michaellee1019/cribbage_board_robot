#pragma once

#include <cstdint>

namespace scorebot {

// Increment this whenever the BLE service shape or game JSON changes. Nodes
// running different protocol revisions are deliberately isolated.
constexpr uint16_t kWireProtocolVersion = 1;

}  // namespace scorebot
