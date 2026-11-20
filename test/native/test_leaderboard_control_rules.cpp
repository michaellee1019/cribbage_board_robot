#include <LeaderboardControlRules.hpp>

#include <cassert>
#include <iostream>
#include <limits>

using scorebot::LeaderboardAction;
using scorebot::LeaderboardActionResult;
using scorebot::Player;
using scorebot::Snapshot;

void selectionRules() {
    const uint8_t redGreen = static_cast<uint8_t>(
        scorebot::playerBit(Player::Red) | scorebot::playerBit(Player::Green));
    assert(scorebot::selectedTurnTarget(Player::Red, redGreen) == Player::Red);
    assert(scorebot::selectedTurnTarget(Player::Blue, redGreen) == Player::None);
    assert(scorebot::nextLeaderboardTarget(Player::None, redGreen) == Player::Red);
    assert(scorebot::nextLeaderboardTarget(Player::Red, redGreen) == Player::Green);
    assert(scorebot::nextLeaderboardTarget(Player::Green, redGreen) == Player::Red);
    assert(scorebot::nextLeaderboardTarget(Player::None, 0) == Player::None);
    assert(scorebot::targetRemainsEligible(Player::Green, redGreen));
    assert(!scorebot::targetRemainsEligible(Player::Blue, redGreen));

    scorebot::Snapshot game{};
    assert(scorebot::setLocalControl(game, Player::Red, true));
    assert(scorebot::setLocalControl(game, Player::Blue, true));
    assert(scorebot::setLocalControl(game, Player::Green, true));
    assert(scorebot::start(game));
    for (const Player expected : {Player::Red, Player::Blue, Player::Green}) {
        assert(game.turn == expected);
        assert(scorebot::selectedTurnTarget(
                   game.turn, scorebot::leaderboardControlMask(game)) ==
               expected);
        assert(scorebot::applyLocal(game, {expected, 0, true}) ==
               scorebot::ApplyResult::Accepted);
    }
    assert(game.turn == Player::Red);
}

Snapshot startedGame() {
    Snapshot game{};
    assert(scorebot::setLocalControl(game, Player::Red, true));
    assert(scorebot::setLocalControl(game, Player::Blue, true));
    assert(scorebot::setLocalControl(game, Player::Green, true));
    assert(scorebot::start(game));
    game.scores = {10, -2, 7, 0};
    game.lastOperation = {11, 12, 13, 14};
    return game;
}

void assertSameSnapshot(const Snapshot& actual, const Snapshot& expected) {
    assert(actual.scores == expected.scores);
    assert(actual.lastOperation == expected.lastOperation);
    assert(actual.connectedMask == expected.connectedMask);
    assert(actual.localControlMask == expected.localControlMask);
    assert(actual.lobbyEnabledMask == expected.lobbyEnabledMask);
    assert(actual.lobbyExplicitMask == expected.lobbyExplicitMask);
    assert(actual.sleepingMask == expected.sleepingMask);
    assert(actual.rosterMask == expected.rosterMask);
    assert(actual.turn == expected.turn);
    assert(actual.started == expected.started);
    assert(actual.version == expected.version);
}

void addAndOkUseExistingScoreAndTurnRules() {
    struct Case {
        LeaderboardAction action;
        Player selected;
        int32_t delta;
        Player nextTurn;
    };
    const Case cases[] = {
        {LeaderboardAction::Add, Player::Red, 5, Player::Red},
        {LeaderboardAction::Add, Player::Red, -15, Player::Red},
        {LeaderboardAction::Add, Player::Blue, -3, Player::Red},
        {LeaderboardAction::Ok, Player::Red, 5, Player::Blue},
        {LeaderboardAction::Ok, Player::Red, -15, Player::Blue},
        {LeaderboardAction::Ok, Player::Red, 0, Player::Blue},
        {LeaderboardAction::Ok, Player::Blue, -3, Player::Red},
        {LeaderboardAction::Ok, Player::Blue, 0, Player::Red},
    };
    for (const Case& test : cases) {
        Snapshot game = startedGame();
        const Snapshot before = game;
        std::array<int32_t, scorebot::kPlayerCount> deltas = {3, -4, 5, -6};
        const size_t index = scorebot::playerIndex(test.selected);
        deltas[index] = test.delta;
        auto expectedDeltas = deltas;
        expectedDeltas[index] = 0;

        assert(scorebot::applyLeaderboardAction(game, deltas, test.selected, test.action) ==
               LeaderboardActionResult::Accepted);
        Snapshot expected = before;
        expected.scores[index] += test.delta;
        expected.turn = test.nextTurn;
        ++expected.version;
        assertSameSnapshot(game, expected);
        assert(deltas == expectedDeltas);
    }
}

void zeroAddDoesNothing() {
    for (const Player selected : {Player::Red, Player::Blue}) {
        Snapshot game = startedGame();
        const Snapshot before = game;
        std::array<int32_t, scorebot::kPlayerCount> deltas = {0, 0, -7, 8};
        const auto beforeDeltas = deltas;
        assert(scorebot::applyLeaderboardAction(game, deltas, selected, LeaderboardAction::Add) ==
               LeaderboardActionResult::NoOp);
        assertSameSnapshot(game, before);
        assert(deltas == beforeDeltas);
    }
}

void rejectedActionsPreserveStateAndAllDrafts() {
    for (const LeaderboardAction action : {LeaderboardAction::Add, LeaderboardAction::Ok,
                                          static_cast<LeaderboardAction>(255)}) {
        for (const Player selected : {Player::Red, Player::Yellow, Player::None,
                                      static_cast<Player>(255)}) {
            for (const bool started : {false, true}) {
                for (const int32_t delta : {0, -7}) {
                    Snapshot game = startedGame();
                    if (!started) {
                        assert(scorebot::resetGame(game));
                    }
                    if (started && selected == Player::Red &&
                        action != static_cast<LeaderboardAction>(255)) {
                        continue;
                    }
                    const Snapshot before = game;
                    std::array<int32_t, scorebot::kPlayerCount> deltas{};
                    deltas.fill(delta);
                    const auto beforeDeltas = deltas;
                    assert(scorebot::applyLeaderboardAction(game, deltas, selected, action) ==
                           LeaderboardActionResult::Rejected);
                    assertSameSnapshot(game, before);
                    assert(deltas == beforeDeltas);
                }
            }
        }
    }
}

void fallbackEligibilityAndTurnOrderStayInGameRules() {
    Snapshot game = startedGame();
    game.localControlMask = 0;
    game.connectedMask = scorebot::playerBit(Player::Red);
    game.sleepingMask = scorebot::playerBit(Player::Blue);
    std::array<int32_t, scorebot::kPlayerCount> deltas = {0, 0, -9, 5};
    // Disconnected roster players remain eligible for leaderboard corrections.
    assert(scorebot::applyLeaderboardAction(game, deltas, Player::Green, LeaderboardAction::Ok) ==
           LeaderboardActionResult::Accepted);
    assert(game.scores[2] == -2);
    assert(game.turn == Player::Red);
    // Sleeping players retain their turn; an unexpected disconnect is skipped.
    assert(scorebot::applyLeaderboardAction(game, deltas, Player::Red, LeaderboardAction::Ok) ==
           LeaderboardActionResult::Accepted);
    assert(game.turn == Player::Blue);
    assert(scorebot::applyLeaderboardAction(game, deltas, Player::Blue, LeaderboardAction::Ok) ==
           LeaderboardActionResult::Accepted);
    assert(game.turn == Player::Red);
    assert(deltas[3] == 5);
}

void scoreLimitsPreserveRejectedDraftsAndAcceptExactBounds() {
    constexpr int32_t low = std::numeric_limits<int32_t>::min();
    constexpr int32_t high = std::numeric_limits<int32_t>::max();
    struct Case {
        int32_t score;
        int32_t delta;
        bool accepted;
    };
    const Case cases[] = {
        {high, 1, false}, {low, -1, false},
        {high, high, false}, {low, low, false},
        {high - 1, 1, true}, {low + 1, -1, true},
        {0, high, true}, {0, low, true},
        {high, low, true}, {low, high, true},
    };
    for (const LeaderboardAction action : {LeaderboardAction::Add, LeaderboardAction::Ok}) {
        for (const Case& test : cases) {
            Snapshot game = startedGame();
            game.scores[0] = test.score;
            const Snapshot before = game;
            std::array<int32_t, scorebot::kPlayerCount> deltas = {test.delta, -3, 4, 5};
            auto expectedDeltas = deltas;
            Snapshot expected = before;
            if (test.accepted) {
                expected.scores[0] = static_cast<int32_t>(
                    static_cast<int64_t>(test.score) + test.delta);
                expected.turn = action == LeaderboardAction::Ok ? Player::Blue : Player::Red;
                ++expected.version;
                expectedDeltas[0] = 0;
            }
            assert(scorebot::applyLeaderboardAction(game, deltas, Player::Red, action) ==
                   (test.accepted ? LeaderboardActionResult::Accepted
                                  : LeaderboardActionResult::Rejected));
            assertSameSnapshot(game, expected);
            assert(deltas == expectedDeltas);
        }
    }
}

int main() {
    selectionRules();
    addAndOkUseExistingScoreAndTurnRules();
    zeroAddDoesNothing();
    rejectedActionsPreserveStateAndAllDrafts();
    fallbackEligibilityAndTurnOrderStayInGameRules();
    scoreLimitsPreserveRejectedDraftsAndAcceptExactBounds();
    std::cout << "Leaderboard-control tests passed\n";
}
