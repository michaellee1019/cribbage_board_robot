#include <LeaderboardUiRules.hpp>

#include <cassert>
#include <string>
#include <iostream>

using scorebot::LeaderboardDisplayMode;

int main() {
    assert(scorebot::leaderboardDisplayMode(false, false, false, false) ==
           LeaderboardDisplayMode::Off);
    assert(scorebot::leaderboardDisplayMode(false, true, false, false) ==
           LeaderboardDisplayMode::Off);
    assert(scorebot::leaderboardDisplayMode(
               false, false, false, false, false, true) ==
           LeaderboardDisplayMode::Pairing);
    assert(scorebot::leaderboardDisplayMode(
               false, true, false, false, false, true) ==
           LeaderboardDisplayMode::LobbyName);
    assert(scorebot::leaderboardDisplayMode(true, true, true, false) ==
           LeaderboardDisplayMode::Score);
    assert(scorebot::leaderboardDisplayMode(true, false, true, false) ==
           LeaderboardDisplayMode::Rejoining);
    assert(scorebot::leaderboardDisplayMode(true, true, false, false) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(true, false, false, false) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(
               false, true, false, true, false, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(
               false, false, false, true, false, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(true, true, true, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(true, false, true, true) ==
           LeaderboardDisplayMode::Sleeping);
    assert(scorebot::leaderboardDisplayMode(true, false, false, true) ==
           LeaderboardDisplayMode::Blank);
    assert(scorebot::leaderboardDisplayMode(
               false, false, false, false, true, true) ==
           LeaderboardDisplayMode::LocalControl);
    assert(scorebot::leaderboardDisplayMode(
               false, true, false, false, true, true) ==
           LeaderboardDisplayMode::LocalControl);
    assert(scorebot::leaderboardDisplayMode(
               false, true, false, false, false, false) ==
           LeaderboardDisplayMode::Off);
    assert(scorebot::leaderboardDisplayMode(
               false, false, false, false, false, false) ==
           LeaderboardDisplayMode::Off);
    assert(scorebot::leaderboardDisplayMode(true, false, true, true, true) ==
           LeaderboardDisplayMode::Score);
    assert(scorebot::leaderboardDisplayMode(
               true, true, true, false, true, true, true) ==
           LeaderboardDisplayMode::Turn);
    assert(scorebot::leaderboardDisplayMode(
               true, false, true, true, true, true, true) ==
           LeaderboardDisplayMode::Turn);
    assert(scorebot::leaderboardDisplayMode(
               true, true, true, false, false, true, true) ==
           LeaderboardDisplayMode::Turn);
    assert(std::string(scorebot::leaderboardLocalPromptFrame(0).data()) == "LOCA");
    assert(std::string(scorebot::leaderboardLocalPromptFrame(999).data()) == "LOCA");
    assert(std::string(scorebot::leaderboardLocalPromptFrame(1000).data()) == "OCAL");
    assert(std::string(scorebot::leaderboardLocalPromptFrame(1999).data()) == "OCAL");
    assert(std::string(scorebot::leaderboardLocalPromptFrame(2000).data()) == "LOCA");
    std::cout << "Leaderboard UI-rule tests passed\n";
}
