#pragma once

#include <cstddef>
#include <cstdint>

namespace scorebot {

// Increment this whenever the BLE service shape or game JSON changes. Nodes
// running different protocol revisions are deliberately isolated.
constexpr uint16_t kWireProtocolVersion = 4;

// The largest valid state snapshot is currently below 300 bytes when every signed
// and unsigned field is rendered at its widest JSON representation. Leave
// room for modest protocol growth while keeping queued events small.
constexpr size_t kMaxWireMessageSize = 352;
static_assert(kMaxWireMessageSize >= 300,
              "wire buffer must hold the widest valid state snapshot");

}  // namespace scorebot
