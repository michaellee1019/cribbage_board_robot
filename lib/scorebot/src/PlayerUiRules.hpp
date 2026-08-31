#pragma once

#include <cstdint>

namespace scorebot {

enum class PlayerDisplayMode : uint8_t { Delta, Pairing, Turn, Idle };

constexpr bool playerTurnLocallyActive(bool authoritativeTurn, bool passPending) {
    return authoritativeTurn && !passPending;
}

// Connection and lobby state are authoritative. Score entry and GO exist only
// during an active game with a live leaderboard.
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
    return PlayerDisplayMode::Idle;
}

constexpr bool playerTurnLightEnabled(
    bool leaderUnavailable, bool gameStarted, bool isMyTurn) {
    return !leaderUnavailable && gameStarted && isMyTurn;
}

}  // namespace scorebot
