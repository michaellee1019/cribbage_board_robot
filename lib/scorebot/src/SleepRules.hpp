#pragma once

#include <cstdint>

namespace scorebot::sleep {

#ifdef SCOREBOT_DEEP_SLEEP_IDLE_MS
// A shared override keeps the short hardware sleep-test build convenient.
inline constexpr uint32_t kPlayerIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
inline constexpr uint32_t kLeaderboardIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
inline constexpr uint32_t kPendingScoreIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
#else
inline constexpr uint32_t kPlayerIdleTimeoutMs = 5u * 60u * 1000u;
inline constexpr uint32_t kLeaderboardIdleTimeoutMs = 10u * 60u * 1000u;
inline constexpr uint32_t kPendingScoreIdleTimeoutMs = 20u * 60u * 1000u;
#endif

inline constexpr uint64_t kStatusPulseIntervalUs = 5ULL * 1000ULL * 1000ULL;
inline constexpr uint32_t kStatusPulseMs = 80;
inline constexpr uint32_t kRejoinAlternateMs = 1000;

constexpr uint32_t idleTimeoutMs(bool isLeaderboard, bool hasPendingScore) {
    if (isLeaderboard) {
        return kLeaderboardIdleTimeoutMs;
    }
    return hasPendingScore ? kPendingScoreIdleTimeoutMs
                           : kPlayerIdleTimeoutMs;
}

constexpr bool isDue(uint32_t now, uint32_t awakeSince,
                     uint32_t lastInteraction, bool blocked,
                     uint32_t timeoutMs) {
    const uint32_t idleSince = lastInteraction == 0 ? awakeSince : lastInteraction;
    return !blocked && static_cast<uint32_t>(now - idleSince) >= timeoutMs;
}

constexpr bool showSavedScore(uint32_t now) {
    return ((now / kRejoinAlternateMs) & 1u) != 0;
}

}  // namespace scorebot::sleep
