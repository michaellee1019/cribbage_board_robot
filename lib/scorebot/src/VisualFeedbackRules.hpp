#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace scorebot {

inline constexpr uint32_t kTurnFadeMs = 1000;
inline constexpr uint32_t kTurnHoldMs = 1000;
inline constexpr uint32_t kTurnCycleMs = kTurnFadeMs * 2 + kTurnHoldMs;
inline constexpr uint8_t kSegmentMaxBrightness = 15;
inline constexpr uint8_t kTurnLightMinLevel = 8;
inline constexpr uint8_t kTurnLightMaxLevel = 32;
inline constexpr uint8_t kLeaderboardIdleLightLevel = 2;
inline constexpr uint32_t kTurnStatusAlternateMs = 1000;
inline constexpr uint32_t kInvalidGestureLightMs = 200;
inline constexpr uint32_t kInvalidGestureRed = 0xff0000;

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
    const uint8_t light = static_cast<uint8_t>(kTurnLightMinLevel +
        (kSegmentMaxBrightness - segment) * (kTurnLightMaxLevel - kTurnLightMinLevel) /
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

constexpr uint32_t turnStatusFrame(uint32_t elapsedMs) {
    return elapsedMs / kTurnStatusAlternateMs;
}

constexpr bool turnStatusShowsScore(uint32_t elapsedMs) {
    // Begin each turn with the turn prompt, then alternate with the total.
    return (turnStatusFrame(elapsedMs) & 1u) != 0;
}

enum class ScoreDisplayStyle : uint8_t { Plain, Framed };

inline std::array<char, 5> scoreDisplayFrame(int32_t score, uint32_t frame,
                                            ScoreDisplayStyle style) {
    // Room for two optional framing dashes, eleven signed digits, and a terminator.
    std::array<char, 14> text{};
    const size_t length = static_cast<size_t>(std::snprintf(
        text.data(), text.size(), style == ScoreDisplayStyle::Framed ? "-%ld-" : "%ld",
        static_cast<long>(score)));
    constexpr size_t width = 4;
    std::array<char, width + 1> display{' ', ' ', ' ', ' ', '\0'};
    const size_t offset = length > width ? frame % (length - width + 1) : 0;
    std::copy_n(text.data() + offset, std::min(length, width), display.data());
    return display;
}

inline std::array<char, 5> activeTurnFrame(int32_t score, uint32_t elapsedMs,
                                          ScoreDisplayStyle style) {
    if (!turnStatusShowsScore(elapsedMs)) return {'B', 'E', 'E', 'F', '\0'};
    return scoreDisplayFrame(score, turnStatusFrame(elapsedMs) / 2, style);
}

constexpr bool invalidGestureLightOn(uint32_t elapsedMs) {
    return elapsedMs < kInvalidGestureLightMs;
}

}  // namespace scorebot
