#pragma once

#include <cstdint>

namespace scorebot::sleep {

#ifndef SCOREBOT_DEEP_SLEEP_IDLE_MS
inline constexpr uint32_t kIdleTimeoutMs = 3u * 60u * 1000u;
#else
inline constexpr uint32_t kIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
#endif

inline constexpr uint64_t kStatusPulseIntervalUs = 5ULL * 1000ULL * 1000ULL;
inline constexpr uint32_t kStatusPulseMs = 80;
inline constexpr uint32_t kRejoinAlternateMs = 1000;

constexpr bool isDue(uint32_t now, uint32_t awakeSince,
                     uint32_t lastInteraction, bool blocked) {
    const uint32_t idleSince = lastInteraction == 0 ? awakeSince : lastInteraction;
    return !blocked && static_cast<uint32_t>(now - idleSince) >= kIdleTimeoutMs;
}

constexpr bool showSavedScore(uint32_t now) {
    return ((now / kRejoinAlternateMs) & 1u) != 0;
}

}  // namespace scorebot::sleep
