#pragma once

#include <cstdint>

namespace scorebot {

enum class PlayerDisplayMode : uint8_t { Delta, Pairing, Turn, Idle, WaitingScore };

inline constexpr uint32_t kPlayerScoreScrollMs = 750;

constexpr bool playerTurnLocallyActive(bool authoritativeTurn, bool passPending) {
    return authoritativeTurn && !passPending;
}

// Score entry and turn status require an active game with a live leaderboard.
constexpr PlayerDisplayMode playerDisplayMode(
    int32_t scoreDelta, bool leaderUnavailable, bool gameStarted, bool isMyTurn) {
    if (leaderUnavailable) {
        return PlayerDisplayMode::Pairing;
    }
    if (!gameStarted) {
        return PlayerDisplayMode::Idle;
    }
    if (scoreDelta != 0) {
        return PlayerDisplayMode::Delta;
    }
    if (isMyTurn) {
        return PlayerDisplayMode::Turn;
    }
    return PlayerDisplayMode::WaitingScore;
}

constexpr bool framedScoreScrolls(int32_t score) {
    return score < -9 || score > 99;
}

constexpr bool playerTurnLightEnabled(
    bool leaderUnavailable, bool gameStarted, bool isMyTurn) {
    return !leaderUnavailable && gameStarted && isMyTurn;
}

}  // namespace scorebot
