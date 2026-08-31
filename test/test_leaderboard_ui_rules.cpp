#include <LeaderboardUiRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::LeaderboardDisplayMode;

int main() {
    assert(scorebot::leaderboardDisplayMode(false, false, false) ==
           LeaderboardDisplayMode::Pairing);
    assert(scorebot::leaderboardDisplayMode(false, true, false) ==
           LeaderboardDisplayMode::LobbyName);
    assert(scorebot::leaderboardDisplayMode(true, true, true) ==
           LeaderboardDisplayMode::Score);
    assert(scorebot::leaderboardDisplayMode(true, false, true) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(true, true, false) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(true, false, false) ==
           LeaderboardDisplayMode::Blank);
    std::cout << "Leaderboard UI-rule tests passed\n";
}
