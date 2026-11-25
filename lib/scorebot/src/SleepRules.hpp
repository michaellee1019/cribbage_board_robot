#pragma once

#include <cstdint>
#include <optional>

namespace scorebot::sleep {

#ifdef SCOREBOT_DEEP_SLEEP_IDLE_MS
inline constexpr uint32_t kUnpairedPlayerIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
inline constexpr uint32_t kSessionIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
inline constexpr uint32_t kPendingScoreIdleTimeoutMs = SCOREBOT_DEEP_SLEEP_IDLE_MS;
#else
inline constexpr uint32_t kUnpairedPlayerIdleTimeoutMs = 5u * 60u * 1000u;
inline constexpr uint32_t kSessionIdleTimeoutMs = 10u * 60u * 1000u;
inline constexpr uint32_t kPendingScoreIdleTimeoutMs = 20u * 60u * 1000u;
#endif

inline constexpr uint32_t kActivityThrottleMs = 100;
inline constexpr uint32_t kGrantRepeatMs = 500;
inline constexpr uint32_t kFollowerGraceMs = 350;
inline constexpr uint32_t kGrantLifetimeMs = 1200;
inline constexpr uint32_t kAuthorityGraceMs = 3000;
inline constexpr uint64_t kStatusPulseIntervalUs = 5ULL * 1000ULL * 1000ULL;
inline constexpr uint32_t kStatusPulseMs = 80;

constexpr bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
    return static_cast<uint32_t>(now - since) >= interval;
}

// A renewed grant after the grace period confirms that the session stayed
// idle. Losing grants or receiving a cancellation restarts that check.
struct SleepGrant {
    std::optional<uint32_t> startedAt;
    uint32_t receivedAt{0};

    void observe(uint32_t now, bool ready) {
        if (!ready) {
            startedAt.reset();
            return;
        }
        if (!startedAt || elapsed(now, receivedAt, kGrantLifetimeMs)) {
            startedAt = now;
        }
        receivedAt = now;
    }

    constexpr bool allowsSleep(uint32_t now) const {
        return startedAt && elapsed(receivedAt, *startedAt, kFollowerGraceMs) &&
               !elapsed(now, receivedAt, kGrantLifetimeMs);
    }
};

struct SleepHandoff {
    std::optional<uint32_t> offeredAt;
    uint32_t sentAt{0};

    constexpr bool grantDue(uint32_t now, bool sessionIdle) const {
        return sessionIdle && (!offeredAt || elapsed(now, sentAt, kGrantRepeatMs));
    }

    void recordGrant(uint32_t now) {
        if (!offeredAt) offeredAt = now;
        sentAt = now;
    }

    constexpr bool allowsSleep(uint32_t now) const {
        return offeredAt && elapsed(now, *offeredAt, kAuthorityGraceMs);
    }
};

constexpr uint32_t offlineIdleTimeoutMs(bool wasInSession, bool hasPendingScore) {
    if (hasPendingScore) return kPendingScoreIdleTimeoutMs;
    return wasInSession ? kSessionIdleTimeoutMs : kUnpairedPlayerIdleTimeoutMs;
}

}  // namespace scorebot::sleep
