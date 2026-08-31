#include <LeaderboardControlRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::Player;

int main() {
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
    std::cout << "Leaderboard-control tests passed\n";
}
