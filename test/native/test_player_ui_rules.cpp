#if !defined(ARDUINO)

#include <GameRules.hpp>
#include <PlayerUiRules.hpp>
#include <VisualFeedbackRules.hpp>

#include <cassert>
#include <iostream>
#include <limits>
#include <string_view>

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

void zeroDeltaShowsTurnIdleOrWaitingScore() {
    assert(scorebot::playerDisplayMode(0, false, true, true) == PlayerDisplayMode::Turn);
    assert(scorebot::playerDisplayMode(0, false, true, false) == PlayerDisplayMode::WaitingScore);
    assert(scorebot::playerDisplayMode(0, false, false, true) == PlayerDisplayMode::Idle);
}

void scoreFramesKeepBothDashesAndAllDigits() {
    auto frame = [](int32_t score, uint32_t index, std::string_view expected) {
        const auto display = scorebot::scoreDisplayFrame(
            score, index, scorebot::ScoreDisplayStyle::Framed);
        assert(std::string_view(display.data()) == expected);
    };

    assert(!scorebot::framedScoreScrolls(0));
    assert(!scorebot::framedScoreScrolls(99));
    assert(!scorebot::framedScoreScrolls(-9));
    assert(scorebot::framedScoreScrolls(100));
    assert(scorebot::framedScoreScrolls(-10));

    frame(0, 0, "-0- ");
    frame(42, 1, "-42-");
    frame(-5, 0, "--5-");
    frame(160, 0, "-160");
    frame(160, 1, "160-");
    frame(160, 2, "-160");
    frame(1234, 1, "1234");
    frame(1234, 2, "234-");

    constexpr int32_t minimum = std::numeric_limits<int32_t>::min();
    frame(minimum, 0, "--21");
    frame(minimum, 1, "-214");
    frame(minimum, 9, "648-");
    frame(minimum, 10, "--21");
}

void activeTurnAlternatesBeefWithFramedScore() {
    auto frame = [](int32_t score, uint32_t time, std::string_view expected) {
        const auto display = scorebot::activeTurnFrame(
            score, time, scorebot::ScoreDisplayStyle::Framed);
        assert(std::string_view(display.data()) == expected);
    };
    frame(42, 0, "BEEF");
    frame(42, 1000, "-42-");
    frame(160, 1000, "-160");
    frame(160, 2000, "BEEF");
    frame(160, 3000, "160-");
}

void turnLightTracksAuthoritativeTurn() {
    assert(scorebot::playerTurnLightEnabled(false, true, true));
    assert(!scorebot::playerTurnLightEnabled(false, true, false));
    assert(!scorebot::playerTurnLightEnabled(false, false, true));
    assert(!scorebot::playerTurnLightEnabled(true, true, true));
}

void draftedScoreStaysVisibleWhenTurnLightComesOn() {
    using namespace scorebot;
    Snapshot game;
    assert(connect(game, Player::Red));
    assert(connect(game, Player::Yellow));
    assert(start(game));
    constexpr int32_t yellowDraft = 10;

    const auto checkYellow = [&](bool expectedLight) {
        const bool yellowTurn = game.turn == Player::Yellow;
        assert(playerDisplayMode(yellowDraft, false, game.started, yellowTurn) ==
               PlayerDisplayMode::Delta);
        assert(playerTurnLightEnabled(false, game.started, yellowTurn) == expectedLight);
    };
    checkYellow(false);
    assert(apply(game, {Player::Red, 0, true, 1}) == ApplyResult::Accepted);
    checkYellow(true);
    assert(!playerTurnLocallyActive(game.turn == Player::Yellow, true));
    assert(playerTurnLightEnabled(false, game.started, game.turn == Player::Yellow));
    assert(apply(game, {Player::Yellow, yellowDraft, true, 1}) == ApplyResult::Accepted);
    assert(!playerTurnLightEnabled(false, game.started, game.turn == Player::Yellow));
}

void pendingPassOptimisticallyEndsLocalTurn() {
    assert(scorebot::playerTurnLocallyActive(true, false));
    assert(!scorebot::playerTurnLocallyActive(true, true));
    assert(!scorebot::playerTurnLocallyActive(false, false));
}

int main() {
    deltaOverridesTurnStatus();
    pairingAndLobbyOverrideScoreEntry();
    zeroDeltaShowsTurnIdleOrWaitingScore();
    scoreFramesKeepBothDashesAndAllDigits();
    activeTurnAlternatesBeefWithFramedScore();
    turnLightTracksAuthoritativeTurn();
    draftedScoreStaysVisibleWhenTurnLightComesOn();
    pendingPassOptimisticallyEndsLocalTurn();
    std::cout << "Player UI-rule tests passed\n";
}

#endif
