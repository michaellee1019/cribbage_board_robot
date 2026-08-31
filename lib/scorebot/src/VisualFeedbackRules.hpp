#pragma once

#include <cstdint>

namespace scorebot {

inline constexpr uint32_t kTurnFadeMs = 1000;
inline constexpr uint32_t kTurnHoldMs = 1000;
inline constexpr uint32_t kTurnCycleMs = kTurnFadeMs * 2 + kTurnHoldMs;
inline constexpr uint8_t kSegmentMaxBrightness = 15;
inline constexpr uint8_t kTurnLightMaxLevel = 32;
inline constexpr uint8_t kLeaderboardIdleLightLevel = 2;

struct TurnPulse {
    uint8_t segmentBrightness;
    uint8_t lightLevel;
};

constexpr TurnPulse turnPulseAt(uint32_t elapsedMs) {
    const uint32_t phase = elapsedMs % kTurnCycleMs;
    uint8_t segment = kSegmentMaxBrightness;
    if (phase < kTurnFadeMs) {
        segment = static_cast<uint8_t>(
            phase * kSegmentMaxBrightness / kTurnFadeMs);
    } else if (phase >= kTurnFadeMs + kTurnHoldMs) {
        segment = static_cast<uint8_t>(
            (kTurnCycleMs - phase) * kSegmentMaxBrightness / kTurnFadeMs);
    }
    const uint8_t light = static_cast<uint8_t>(
        (kSegmentMaxBrightness - segment) * kTurnLightMaxLevel /
        kSegmentMaxBrightness);
    return {segment, light};
}

constexpr uint8_t leaderboardLightLevel(uint8_t segmentBrightness) {
    const uint8_t clamped = segmentBrightness > kSegmentMaxBrightness
                                ? kSegmentMaxBrightness
                                : segmentBrightness;
    return static_cast<uint8_t>(
        kLeaderboardIdleLightLevel +
        clamped * (kTurnLightMaxLevel - kLeaderboardIdleLightLevel) /
            kSegmentMaxBrightness);
}

}  // namespace scorebot
