#if !defined(ARDUINO)

#include <PlayerUiRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::PlayerDisplayMode;

void deltaOverridesTurnStatus() {
    assert(scorebot::playerDisplayMode(7, false, true, true) == PlayerDisplayMode::Delta);
    assert(scorebot::playerDisplayMode(-2, false, true, true) == PlayerDisplayMode::Delta);
}

void pairingAndLobbyOverrideScoreEntry() {
    assert(scorebot::playerDisplayMode(5, true, true, true) == PlayerDisplayMode::Pairing);
    assert(scorebot::playerDisplayMode(0, true, true, true) == PlayerDisplayMode::Pairing);
    assert(scorebot::playerDisplayMode(5, false, false, true) == PlayerDisplayMode::Idle);
}

void zeroDeltaShowsTurnOrIdle() {
    assert(scorebot::playerDisplayMode(0, false, true, true) == PlayerDisplayMode::Turn);
    assert(scorebot::playerDisplayMode(0, false, true, false) == PlayerDisplayMode::Idle);
    assert(scorebot::playerDisplayMode(0, false, false, true) == PlayerDisplayMode::Idle);
}

void turnLightTracksAuthoritativeTurn() {
    assert(scorebot::playerTurnLightEnabled(false, true, true));
    assert(!scorebot::playerTurnLightEnabled(false, true, false));
    assert(!scorebot::playerTurnLightEnabled(false, false, true));
    assert(!scorebot::playerTurnLightEnabled(true, true, true));
}

void pendingPassOptimisticallyEndsLocalTurn() {
    assert(scorebot::playerTurnLocallyActive(true, false));
    assert(!scorebot::playerTurnLocallyActive(true, true));
    assert(!scorebot::playerTurnLocallyActive(false, false));
}

int main() {
    deltaOverridesTurnStatus();
    pairingAndLobbyOverrideScoreEntry();
    zeroDeltaShowsTurnOrIdle();
    turnLightTracksAuthoritativeTurn();
    pendingPassOptimisticallyEndsLocalTurn();
    std::cout << "Player UI-rule tests passed\n";
}

#endif
