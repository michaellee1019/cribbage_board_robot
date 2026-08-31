#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace scorebot {

enum class Player : uint8_t { Red = 0, Blue, Green, White, None };

constexpr size_t kPlayerCount = 4;

constexpr bool isPlayer(Player player) {
    return static_cast<size_t>(player) < kPlayerCount;
}

constexpr size_t playerIndex(Player player) {
    return static_cast<size_t>(player);
}

struct Snapshot {
    std::array<int32_t, kPlayerCount> scores{};
    std::array<uint32_t, kPlayerCount> lastOperation{};
    uint8_t connectedMask{0};
    uint8_t sleepingMask{0};
    uint8_t rosterMask{0};
    Player turn{Player::None};
    bool started{false};
    uint32_t version{0};
};

struct ScoreAction {
    Player player{Player::None};
    int32_t scoreDelta{0};
    bool passesTurn{false};
    uint32_t operationId{0};
};

enum class ApplyResult {
    Accepted,
    AcceptedWithoutTurnChange,
    GameNotStarted,
    UnknownPlayer,
    NotConnected,
    Duplicate,
    ScoreOutOfRange,
};

constexpr bool isConnected(const Snapshot& snapshot, Player player) {
    return isPlayer(player) && (snapshot.connectedMask & (1u << playerIndex(player))) != 0;
}

inline Player nextConnected(Player current, const Snapshot& snapshot) {
    if (snapshot.connectedMask == 0) {
        return Player::None;
    }
    size_t start = isPlayer(current) ? playerIndex(current) : kPlayerCount - 1;
    for (size_t offset = 1; offset <= kPlayerCount; ++offset) {
        const Player candidate = static_cast<Player>((start + offset) % kPlayerCount);
        if (isConnected(snapshot, candidate)) {
            return candidate;
        }
    }
    return Player::None;
}

inline Player nextTurnParticipant(Player current, const Snapshot& snapshot) {
    // Intentional sleep removes a player from the live connection mask, but it
    // must not remove that player from the frozen turn order. An unexpected
    // disconnect has no sleeping bit and is still skipped.
    const uint8_t eligibleMask = snapshot.connectedMask |
        (snapshot.sleepingMask & snapshot.rosterMask);
    if (eligibleMask == 0) {
        return Player::None;
    }
    const size_t start = isPlayer(current) ? playerIndex(current) : kPlayerCount - 1;
    for (size_t offset = 1; offset <= kPlayerCount; ++offset) {
        const Player candidate = static_cast<Player>((start + offset) % kPlayerCount);
        if ((eligibleMask & (1u << playerIndex(candidate))) != 0) {
            return candidate;
        }
    }
    return Player::None;
}

inline bool start(Snapshot& snapshot) {
    if (snapshot.started || snapshot.connectedMask == 0) {
        return false;
    }
    snapshot.started = true;
    snapshot.rosterMask = snapshot.connectedMask;
    snapshot.turn = nextConnected(Player::None, snapshot);
    ++snapshot.version;
    return true;
}

inline bool resetGame(Snapshot& snapshot) {
    if (!snapshot.started) {
        return false;
    }
    snapshot.scores.fill(0);
    // Keep operation high-water marks so delayed requests from the previous
    // game cannot be applied to the next one.
    snapshot.rosterMask = 0;
    snapshot.turn = Player::None;
    snapshot.started = false;
    ++snapshot.version;
    return true;
}

inline bool connect(Snapshot& snapshot, Player player) {
    if (!isPlayer(player) || isConnected(snapshot, player) ||
        (snapshot.started && (snapshot.rosterMask & (1u << playerIndex(player))) == 0)) {
        return false;
    }
    snapshot.connectedMask |= 1u << playerIndex(player);
    ++snapshot.version;
    return true;
}

inline bool disconnect(Snapshot& snapshot, Player player) {
    if (!isPlayer(player) || !isConnected(snapshot, player)) {
        return false;
    }
    snapshot.connectedMask &= ~(1u << playerIndex(player));
    ++snapshot.version;
    return true;
}

inline ApplyResult apply(Snapshot& snapshot, const ScoreAction& action) {
    if (!snapshot.started) {
        return ApplyResult::GameNotStarted;
    }
    if (!isPlayer(action.player) || action.operationId == 0) {
        return ApplyResult::UnknownPlayer;
    }
    const size_t index = playerIndex(action.player);
    if (!isConnected(snapshot, action.player)) {
        return ApplyResult::NotConnected;
    }
    if (action.operationId <= snapshot.lastOperation[index]) {
        return ApplyResult::Duplicate;
    }
    const int64_t nextScore = static_cast<int64_t>(snapshot.scores[index]) + action.scoreDelta;
    if (nextScore < std::numeric_limits<int32_t>::min() ||
        nextScore > std::numeric_limits<int32_t>::max()) {
        snapshot.lastOperation[index] = action.operationId;
        ++snapshot.version;
        return ApplyResult::ScoreOutOfRange;
    }
    snapshot.scores[index] = static_cast<int32_t>(nextScore);
    snapshot.lastOperation[index] = action.operationId;
    const bool turnMayChange = action.passesTurn && snapshot.turn == action.player;
    if (turnMayChange) {
        snapshot.turn = nextTurnParticipant(action.player, snapshot);
    }
    ++snapshot.version;
    return action.passesTurn && !turnMayChange
               ? ApplyResult::AcceptedWithoutTurnChange
               : ApplyResult::Accepted;
}

}  // namespace scorebot
