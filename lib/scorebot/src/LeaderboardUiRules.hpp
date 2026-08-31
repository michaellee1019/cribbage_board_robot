#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace scorebot {

inline constexpr uint32_t kLocalPromptScrollStepMs = 1000;

enum class LeaderboardDisplayMode {
    Pairing,
    LobbyName,
    LocalControl,
    Off,
    Turn,
    Score,
    Rejoining,
    Sleeping,
    Blank,
};

constexpr LeaderboardDisplayMode leaderboardDisplayMode(
    bool gameStarted, bool connected, bool inRoster, bool sleeping,
    bool locallyControlled = false, bool lobbyEnabled = false,
    bool locallyActiveTurn = false) {
    if (!gameStarted) {
        if (!lobbyEnabled) {
            return LeaderboardDisplayMode::Off;
        }
        if (locallyControlled) {
            return LeaderboardDisplayMode::LocalControl;
        }
        if (sleeping) {
            return LeaderboardDisplayMode::Sleeping;
        }
        return connected ? LeaderboardDisplayMode::LobbyName
                         : LeaderboardDisplayMode::Pairing;
    }
    if (!inRoster) {
        return LeaderboardDisplayMode::Blank;
    }
    if (locallyActiveTurn) {
        return LeaderboardDisplayMode::Turn;
    }
    if (locallyControlled) {
        return LeaderboardDisplayMode::Score;
    }
    if (sleeping) {
        return LeaderboardDisplayMode::Sleeping;
    }
    return connected ? LeaderboardDisplayMode::Score
                     : LeaderboardDisplayMode::Rejoining;
}

constexpr std::array<char, 5> leaderboardLocalPromptFrame(uint32_t elapsedMs) {
    constexpr std::array<std::array<char, 5>, 2> frames = {{
        {'L', 'O', 'C', 'A', '\0'},
        {'O', 'C', 'A', 'L', '\0'},
    }};
    const size_t frame = static_cast<size_t>(
        (elapsedMs / kLocalPromptScrollStepMs) % frames.size());
    return frames[frame];
}

}  // namespace scorebot
