#pragma once

#include <GameRules.hpp>

#include <array>
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

enum class LeaderboardAction { Add, Ok };
enum class LeaderboardActionResult { NoOp, Rejected, Accepted };

// Operate on the selected draft independently of button decoding. The adapter
// owns encoder/display effects and the authoritative commit after acceptance.
inline LeaderboardActionResult applyLeaderboardAction(
    Snapshot& snapshot, std::array<int32_t, kPlayerCount>& deltas,
    Player selected, LeaderboardAction action) {
    if ((action != LeaderboardAction::Add && action != LeaderboardAction::Ok) ||
        !snapshot.started ||
        !targetRemainsEligible(selected, leaderboardControlMask(snapshot))) {
        return LeaderboardActionResult::Rejected;
    }
    const size_t index = playerIndex(selected);
    if (action == LeaderboardAction::Add && deltas[index] == 0) {
        return LeaderboardActionResult::NoOp;
    }
    const ApplyResult result = applyLocal(
        snapshot, {selected, deltas[index], action == LeaderboardAction::Ok});
    if (result != ApplyResult::Accepted &&
        result != ApplyResult::AcceptedWithoutTurnChange) {
        return LeaderboardActionResult::Rejected;
    }
    deltas[index] = 0;
    return LeaderboardActionResult::Accepted;
}

}  // namespace scorebot
