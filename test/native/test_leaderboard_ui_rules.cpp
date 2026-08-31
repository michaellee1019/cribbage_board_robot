#include <LeaderboardUiRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::LeaderboardDisplayMode;

int main() {
    assert(scorebot::leaderboardDisplayMode(false, false, false, false) ==
           LeaderboardDisplayMode::Pairing);
    assert(scorebot::leaderboardDisplayMode(false, true, false, false) ==
           LeaderboardDisplayMode::LobbyName);
    assert(scorebot::leaderboardDisplayMode(true, true, true, false) ==
           LeaderboardDisplayMode::Score);
    assert(scorebot::leaderboardDisplayMode(true, false, true, false) ==
           LeaderboardDisplayMode::Rejoining);
    assert(scorebot::leaderboardDisplayMode(true, true, false, false) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(true, false, false, false) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(false, true, false, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(false, false, false, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(true, true, true, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(true, false, true, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(true, false, false, true) ==
           LeaderboardDisplayMode::Blank);
    std::cout << "Leaderboard UI-rule tests passed\n";
}
