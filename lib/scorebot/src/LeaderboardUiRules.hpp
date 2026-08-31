#pragma once

namespace scorebot {

enum class LeaderboardDisplayMode { Pairing, LobbyName, Score, Blank };

constexpr LeaderboardDisplayMode leaderboardDisplayMode(
    bool gameStarted, bool connected, bool inRoster) {
    if (!gameStarted) {
        return connected ? LeaderboardDisplayMode::LobbyName
                         : LeaderboardDisplayMode::Pairing;
    }
    return connected && inRoster ? LeaderboardDisplayMode::Score
                                 : LeaderboardDisplayMode::Blank;
}

}  // namespace scorebot
