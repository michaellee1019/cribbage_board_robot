#pragma once

#include <cstdint>

namespace scorebot::ble_power {

inline constexpr int8_t kInitialConnectionPowerDbm = 3;
inline constexpr int8_t kIntermediateConnectionPowerDbm = 6;
inline constexpr int8_t kMaximumConnectionPowerDbm = 9;
inline constexpr uint32_t kDisconnectWindowMs = 60u * 1000u;
inline constexpr uint8_t kDisconnectThreshold = 3;

struct DisconnectWindow {
    uint32_t startedMs{0};
    uint8_t count{0};
};

struct DisconnectResult {
    DisconnectWindow window;
    bool increasePower;
};

constexpr DisconnectResult recordUnexpectedDisconnect(
    DisconnectWindow window, uint32_t now) {
    if (window.startedMs == 0 ||
        static_cast<uint32_t>(now - window.startedMs) > kDisconnectWindowMs) {
        return {{now, 1}, false};
    }
    ++window.count;
    if (window.count < kDisconnectThreshold) {
        return {window, false};
    }
    return {{now, 0}, true};
}

constexpr int8_t nextConnectionPowerDbm(int8_t currentDbm) {
    if (currentDbm < kIntermediateConnectionPowerDbm) {
        return kIntermediateConnectionPowerDbm;
    }
    return kMaximumConnectionPowerDbm;
}

}  // namespace scorebot::ble_power
