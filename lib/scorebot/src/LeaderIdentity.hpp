#pragma once

#include <cstdint>

// Supply BOTH as build definitions to replace the hardware leaderboard. The
// address is the Pi adapter's actual, stable PUBLIC Bluetooth address, in
// display/network byte order (48-bit integer), not an ESP32 base MAC.
#if defined(SCOREBOT_LEADER_NODE_ID) != defined(SCOREBOT_LEADER_BLUETOOTH_MAC)
#error "Configure both SCOREBOT_LEADER_NODE_ID and SCOREBOT_LEADER_BLUETOOTH_MAC"
#endif

namespace scorebot {
#if defined(SCOREBOT_LEADER_NODE_ID)
constexpr bool kExternalLeader = true;
static_assert(SCOREBOT_LEADER_NODE_ID > 0 && SCOREBOT_LEADER_NODE_ID <= UINT32_MAX,
              "leader node id must be a nonzero uint32_t");
static_assert(SCOREBOT_LEADER_BLUETOOTH_MAC > 0 &&
                  SCOREBOT_LEADER_BLUETOOTH_MAC <= 0xffffffffffffULL,
              "leader Bluetooth address must be a nonzero 48-bit public address");
constexpr uint32_t kLeaderNodeId = SCOREBOT_LEADER_NODE_ID;
constexpr uint64_t kLeaderBluetoothMac = SCOREBOT_LEADER_BLUETOOTH_MAC;
#else
constexpr bool kExternalLeader = false;
constexpr uint32_t kLeaderNodeId = 2934577084u;
constexpr uint64_t kLeaderBluetoothMac = 0;  // Default uses ESP32 base + offset.
#endif
}  // namespace scorebot
