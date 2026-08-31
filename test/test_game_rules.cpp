#if !defined(ARDUINO)

#include <GameRules.hpp>

#include <cassert>
#include <iostream>
#include <limits>

using scorebot::ApplyResult;
using scorebot::Player;
using scorebot::ScoreAction;
using scorebot::Snapshot;

Snapshot startedThreePlayerGame() {
    Snapshot game{};
    game.connectedMask = (1u << 0) | (1u << 1) | (1u << 2);
    assert(scorebot::start(game));
    assert(game.turn == Player::Red);
    return game;
}

void lifecycleSelectsOnlyConnectedPlayers() {
    Snapshot game{};
    assert(!scorebot::start(game));
    assert(game.turn == Player::None);
    assert(!scorebot::connect(game, Player::None));
    assert(scorebot::connect(game, Player::Blue));
    assert(scorebot::connect(game, Player::White));
    assert(!scorebot::connect(game, Player::Blue));
    assert(game.version == 2);
    assert(scorebot::start(game));
    assert(game.turn == Player::Blue);
    assert(!scorebot::start(game));
    assert(scorebot::nextConnected(Player::Blue, game) == Player::White);
    assert(scorebot::nextConnected(Player::White, game) == Player::Blue);
}

void scoresMayBeCorrectedOutOfTurn() {
    Snapshot game = startedThreePlayerGame();
    assert(scorebot::apply(game, {Player::Blue, -2, false, 1}) == ApplyResult::Accepted);
    assert(scorebot::apply(game, {Player::Green, 3, false, 1}) == ApplyResult::Accepted);
    assert(game.scores[1] == -2 && game.scores[2] == 3);
    assert(game.turn == Player::Red);
}

void outOfTurnOkScoresWithoutChangingTurn() {
    Snapshot game = startedThreePlayerGame();
    const uint32_t version = game.version;
    const auto result = scorebot::apply(game, {Player::Blue, 7, true, 1});
    assert(result == ApplyResult::AcceptedWithoutTurnChange);
    assert(game.scores[1] == 7);
    assert(game.turn == Player::Red);
    assert(game.lastOperation[1] == 1);
    assert(game.version == version + 1);
    assert(scorebot::apply(game, {Player::Blue, 7, true, 1}) == ApplyResult::Duplicate);
    assert(game.scores[1] == 7);
}

void acceptedOperationIsIdempotent() {
    Snapshot game = startedThreePlayerGame();
    assert(scorebot::apply(game, {Player::Red, 5, true, 9}) == ApplyResult::Accepted);
    assert(game.scores[0] == 5);
    assert(game.turn == Player::Blue);
    assert(scorebot::apply(game, {Player::Red, 5, true, 9}) == ApplyResult::Duplicate);
    assert(game.scores[0] == 5);
    assert(game.turn == Player::Blue);
}

void disconnectionSkipsTheCurrentTurn() {
    Snapshot game = startedThreePlayerGame();
    const uint32_t version = game.version;
    assert(scorebot::disconnect(game, Player::Red));
    assert(game.turn == Player::Blue);
    assert(game.connectedMask == ((1u << 1) | (1u << 2)));
    assert(game.version == version + 1);
    assert(!scorebot::disconnect(game, Player::Red));
    assert(game.version == version + 1);
}

void startFreezesRosterButAllowsReconnection() {
    Snapshot game = startedThreePlayerGame();
    assert(game.rosterMask == ((1u << 0) | (1u << 1) | (1u << 2)));
    assert(!scorebot::connect(game, Player::White));
    assert(scorebot::disconnect(game, Player::Blue));
    assert((game.rosterMask & (1u << 1)) != 0);
    assert(scorebot::connect(game, Player::Blue));
}

void resetStartsCleanLobbyWithoutAdmittingDelayedOperations() {
    Snapshot game = startedThreePlayerGame();
    assert(scorebot::apply(game, {Player::Red, 5, true, 9}) == ApplyResult::Accepted);
    const uint8_t connected = game.connectedMask;
    const uint32_t version = game.version;
    assert(scorebot::resetGame(game));
    assert(!game.started);
    assert(game.turn == Player::None);
    assert(game.rosterMask == 0);
    assert(game.connectedMask == connected);
    assert((game.scores == std::array<int32_t, 4>{}));
    assert(game.lastOperation[0] == 9);
    assert(game.version == version + 1);
    assert(!scorebot::resetGame(game));
}

void reconnectPreservesFrozenTurn() {
    Snapshot game = startedThreePlayerGame();
    assert(scorebot::disconnect(game, Player::Red));
    assert(scorebot::disconnect(game, Player::Blue));
    assert(scorebot::disconnect(game, Player::Green));
    assert(game.turn == Player::None);
    assert(scorebot::connect(game, Player::Blue));
    assert(game.turn == Player::Blue);

    // This also models a leaderboard restart with a saved, unavailable turn.
    game.connectedMask = 0;
    game.turn = Player::Red;
    assert(scorebot::connect(game, Player::Green));
    assert(game.turn == Player::Green);
}

void normalPassScoresAndAdvancesExactlyOnce() {
    Snapshot game = startedThreePlayerGame();
    const uint32_t version = game.version;
    assert(scorebot::apply(game, {Player::Red, 15, true, 10}) == ApplyResult::Accepted);
    assert(game.scores[0] == 15);
    assert(game.turn == Player::Blue);
    assert(game.version == version + 1);

    // A correction from Blue does not grant it a turn; its later pass does.
    assert(scorebot::apply(game, {Player::Blue, -1, false, 7}) == ApplyResult::Accepted);
    assert(game.turn == Player::Blue);
    assert(scorebot::apply(game, {Player::Blue, 4, true, 8}) == ApplyResult::Accepted);
    assert(game.scores[1] == 3);
    assert(game.turn == Player::Green);
}

void passWrapsAndSkipsDisconnectedRoles() {
    Snapshot game = startedThreePlayerGame();
    assert(scorebot::apply(game, {Player::Red, 0, true, 1}) == ApplyResult::Accepted);
    assert(scorebot::apply(game, {Player::Blue, 0, true, 1}) == ApplyResult::Accepted);
    assert(scorebot::apply(game, {Player::Green, 0, true, 1}) == ApplyResult::Accepted);
    assert(game.turn == Player::Red);  // White is not connected.
}

void operationsRequireAnActiveConnectedPlayer() {
    Snapshot game{};
    assert(scorebot::apply(game, {Player::Red, 1, false, 1}) == ApplyResult::GameNotStarted);
    game = startedThreePlayerGame();
    assert(scorebot::apply(game, {Player::White, 1, false, 1}) == ApplyResult::NotConnected);
    assert(scorebot::apply(game, {Player::Red, 1, false, 0}) == ApplyResult::UnknownPlayer);
}

void scoresCannotOverflow() {
    Snapshot game = startedThreePlayerGame();
    game.scores[0] = std::numeric_limits<int32_t>::max();
    assert(scorebot::apply(game, {Player::Red, 1, false, 1}) == ApplyResult::ScoreOutOfRange);
    assert(game.scores[0] == std::numeric_limits<int32_t>::max());
}

void completeMultiplayerLifecycle() {
    Snapshot game{};
    assert(scorebot::connect(game, Player::Red));
    assert(scorebot::connect(game, Player::Blue));
    assert(scorebot::start(game));
    assert(game.turn == Player::Red);

    // Blue's out-of-turn OK commits its score without taking or passing turn.
    assert(scorebot::apply(game, {Player::Blue, 5, true, 1}) ==
           ApplyResult::AcceptedWithoutTurnChange);
    assert(game.scores[1] == 5);
    assert(game.turn == Player::Red);

    assert(scorebot::apply(game, {Player::Red, 3, true, 1}) == ApplyResult::Accepted);
    assert(game.turn == Player::Blue);
    assert(scorebot::apply(game, {Player::Blue, -1, false, 2}) == ApplyResult::Accepted);
    assert(game.turn == Player::Blue);
    assert(scorebot::apply(game, {Player::Blue, 2, true, 3}) == ApplyResult::Accepted);
    assert(game.scores[1] == 6);
    assert(game.turn == Player::Red);
    assert(!scorebot::connect(game, Player::Green));

    assert(scorebot::resetGame(game));
    assert(scorebot::connect(game, Player::Green));
    assert(scorebot::start(game));
    assert(game.scores[0] == 0 && game.scores[1] == 0 && game.scores[2] == 0);
    assert(game.turn == Player::Red);
    // A delayed request from the previous game remains below the high-water mark.
    assert(scorebot::apply(game, {Player::Blue, 5, false, 1}) == ApplyResult::Duplicate);
}

int main() {
    lifecycleSelectsOnlyConnectedPlayers();
    scoresMayBeCorrectedOutOfTurn();
    outOfTurnOkScoresWithoutChangingTurn();
    acceptedOperationIsIdempotent();
    disconnectionSkipsTheCurrentTurn();
    startFreezesRosterButAllowsReconnection();
    resetStartsCleanLobbyWithoutAdmittingDelayedOperations();
    reconnectPreservesFrozenTurn();
    normalPassScoresAndAdvancesExactlyOnce();
    passWrapsAndSkipsDisconnectedRoles();
    operationsRequireAnActiveConnectedPlayer();
    scoresCannotOverflow();
    completeMultiplayerLifecycle();
    std::cout << "Game-rule tests passed\n";
}

#endif
