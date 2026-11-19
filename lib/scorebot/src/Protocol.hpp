#pragma once

#include <cstddef>
#include <cstdint>

namespace scorebot {

// Increment this whenever the BLE service shape or game JSON changes. Nodes
// running different protocol revisions are deliberately isolated.
constexpr uint16_t kWireProtocolVersion = 5;

// State includes four controller operation marks alongside the scores. A
// 512-byte ATT attribute accommodates the widest compact JSON snapshot.
constexpr size_t kMaxWireMessageSize = 512;
static_assert(kMaxWireMessageSize >= 300,
              "wire buffer must hold the widest valid state snapshot");

}  // namespace scorebot
