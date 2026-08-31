#pragma once

#include <GameRules.hpp>

#include <cstdint>

namespace scorebot {

constexpr Player selectedTurnTarget(Player turn, uint8_t controlMask) {
    return (controlMask & playerBit(turn)) != 0 ? turn : Player::None;
}

constexpr Player nextLeaderboardTarget(Player current, uint8_t controlMask) {
    return nextPlayerInMask(current, controlMask);
}

constexpr bool targetRemainsEligible(Player target, uint8_t controlMask) {
    return isPlayer(target) && (controlMask & playerBit(target)) != 0;
}

}  // namespace scorebot
