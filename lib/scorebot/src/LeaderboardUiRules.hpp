#pragma once

namespace scorebot {

enum class LeaderboardDisplayMode { Pairing, LobbyName, Score, Rejoining, Sleeping, Blank };

constexpr LeaderboardDisplayMode leaderboardDisplayMode(
    bool gameStarted, bool connected, bool inRoster, bool sleeping) {
    if (!gameStarted) {
        if (sleeping) {
            return LeaderboardDisplayMode::Sleeping;
        }
        return connected ? LeaderboardDisplayMode::LobbyName
                         : LeaderboardDisplayMode::Pairing;
    }
    if (!inRoster) {
        return LeaderboardDisplayMode::Blank;
    }
    if (sleeping) {
        return LeaderboardDisplayMode::Sleeping;
    }
    return connected ? LeaderboardDisplayMode::Score
                     : LeaderboardDisplayMode::Rejoining;
}

}  // namespace scorebot
