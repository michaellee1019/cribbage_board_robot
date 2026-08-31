#pragma once

namespace scorebot {

enum class LeaderboardDisplayMode { Pairing, LobbyName, Score, Rejoining, Blank };

constexpr LeaderboardDisplayMode leaderboardDisplayMode(
    bool gameStarted, bool connected, bool inRoster) {
    if (!gameStarted) {
        return connected ? LeaderboardDisplayMode::LobbyName
                         : LeaderboardDisplayMode::Pairing;
    }
    if (!inRoster) {
        return LeaderboardDisplayMode::Blank;
    }
    return connected ? LeaderboardDisplayMode::Score
                     : LeaderboardDisplayMode::Rejoining;
}

}  // namespace scorebot
