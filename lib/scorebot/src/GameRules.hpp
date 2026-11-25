#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace scorebot {

enum class Player : uint8_t { Red = 0, Blue, Green, Yellow, None, White = Yellow };
enum class LobbyMode : uint8_t { Local = 0, Physical, Off };

constexpr size_t kPlayerCount = 4;
inline constexpr std::array<uint8_t, kPlayerCount + 1> kPermutationCount{1, 1, 2, 6, 24};
inline constexpr uint8_t kDefaultTurnOrder = 0;
inline constexpr uint8_t kTurnOrderCount = kPermutationCount.back();
using TurnOrder = std::array<Player, kPlayerCount>;
using TurnCompletions = std::array<uint16_t, kPlayerCount>;

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
    uint8_t localControlMask{0};
    uint8_t lobbyEnabledMask{0};
    uint8_t lobbyExplicitMask{0};
    uint8_t sleepingMask{0};
    uint8_t rosterMask{0};
    Player turn{Player::None};
    bool started{false};
    uint32_t version{0};
    uint8_t turnOrder{kDefaultTurnOrder};
    TurnCompletions completedTurns{};
};

struct ScoreAction {
    Player player{Player::None};
    int32_t scoreDelta{0};
    bool passesTurn{false};
    uint32_t operationId{0};
};

struct LocalScoreAction {
    Player player{Player::None};
    int32_t scoreDelta{0};
    bool passesTurn{false};
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

constexpr uint8_t playerBit(Player player) {
    return isPlayer(player) ? static_cast<uint8_t>(1u << playerIndex(player)) : 0;
}

constexpr bool isTurnOrder(int32_t code) {
    return code >= 0 && code < kTurnOrderCount;
}

// A full color order has 24 possible ranks. Filtering that canonical order
// keeps one compact representation even when roster availability changes.
constexpr TurnOrder decodeTurnOrder(uint8_t code) {
    TurnOrder remaining{Player::Red, Player::Blue, Player::Green, Player::Yellow};
    TurnOrder order{};
    uint8_t rank = isTurnOrder(code) ? code : kDefaultTurnOrder;
    for (size_t position = 0; position < kPlayerCount; ++position) {
        const size_t count = kPlayerCount - position;
        const uint8_t blockSize = kPermutationCount[count - 1];
        const size_t selected = rank / blockSize;
        order[position] = remaining[selected];
        rank %= blockSize;
        for (size_t index = selected; index + 1 < count; ++index) {
            remaining[index] = remaining[index + 1];
        }
    }
    return order;
}

constexpr uint8_t encodeTurnOrder(const TurnOrder& order) {
    uint8_t rank = 0;
    for (size_t position = 0; position < kPlayerCount; ++position) {
        for (size_t index = position + 1; index < kPlayerCount; ++index) {
            if (order[index] < order[position]) {
                rank += kPermutationCount[kPlayerCount - position - 1];
            }
        }
    }
    return rank;
}

inline uint8_t rotatedTurnOrder(uint8_t code, uint8_t participantMask, int32_t delta) {
    const TurnOrder current = decodeTurnOrder(code);
    TurnOrder next{};
    const auto end = std::copy_if(current.begin(), current.end(), next.begin(),
        [participantMask](Player player) { return (participantMask & playerBit(player)) != 0; });
    const int32_t offset = delta % kPermutationCount[static_cast<size_t>(end - next.begin())];
    if (offset == 0) {
        return code;
    }
    for (int32_t step = 0; step < offset; ++step) {
        std::next_permutation(next.begin(), end);
    }
    for (int32_t step = 0; step > offset; --step) {
        std::prev_permutation(next.begin(), end);
    }
    TurnOrder reordered = current;
    auto player = next.begin();
    for (Player& slot : reordered) {
        if ((participantMask & playerBit(slot)) != 0) {
            slot = *player++;
        }
    }
    return encodeTurnOrder(reordered);
}

constexpr bool draftHasExpired(uint32_t now, uint32_t lastLocalInteraction, Player player,
                               const TurnCompletions& baseline, const TurnCompletions& current) {
    constexpr uint32_t kDraftIdleMs = 60u * 1000u;
    if (!isPlayer(player) || now - lastLocalInteraction <= kDraftIdleMs) {
        return false;
    }
    for (size_t index = 0; index < kPlayerCount; ++index) {
        // Compare modular counts to tolerate rollover without treating a small
        // rollback after an authority power loss as a completed turn.
        const auto advances = static_cast<uint16_t>(current[index] - baseline[index]);
        if (index != playerIndex(player) && advances != 0 && advances < 0x8000) {
            return true;
        }
    }
    return false;
}

constexpr uint8_t lobbyParticipantMask(const Snapshot& snapshot) {
    return static_cast<uint8_t>(
        snapshot.lobbyEnabledMask &
        (snapshot.connectedMask | snapshot.localControlMask));
}

constexpr LobbyMode lobbyModeFor(const Snapshot& snapshot, Player player) {
    const uint8_t bit = playerBit(player);
    if (bit == 0 || (snapshot.lobbyEnabledMask & bit) == 0) {
        return LobbyMode::Off;
    }
    return (snapshot.localControlMask & bit) != 0
               ? LobbyMode::Local
               : LobbyMode::Physical;
}

constexpr LobbyMode rotatedLobbyMode(LobbyMode current, int32_t delta) {
    constexpr int32_t count = 3;
    int32_t value = static_cast<int32_t>(current);
    int32_t offset = delta % count;
    value = (value + offset + count) % count;
    return static_cast<LobbyMode>(value);
}

constexpr uint8_t leaderboardControlMask(const Snapshot& snapshot) {
    if (!snapshot.started) {
        return snapshot.localControlMask;
    }
    // Once the roster is frozen, the leaderboard is a universal fallback for
    // every participant. This keeps the active turn operable regardless of a
    // physical board's connection, sleep, or pairing state.
    return snapshot.rosterMask;
}

constexpr bool isLocallyControllable(const Snapshot& snapshot, Player player) {
    return (leaderboardControlMask(snapshot) & playerBit(player)) != 0;
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

constexpr Player nextPlayerInMask(
    Player current, uint8_t mask, uint8_t turnOrder = kDefaultTurnOrder) {
    if (mask == 0) {
        return Player::None;
    }
    const TurnOrder order = decodeTurnOrder(turnOrder);
    size_t start = kPlayerCount - 1;
    for (size_t index = 0; index < kPlayerCount; ++index) {
        if (order[index] == current) {
            start = index;
            break;
        }
    }
    for (size_t offset = 1; offset <= kPlayerCount; ++offset) {
        const Player candidate = order[(start + offset) % kPlayerCount];
        if ((mask & playerBit(candidate)) != 0) {
            return candidate;
        }
    }
    return Player::None;
}

inline Player nextTurnParticipant(
    Player current, const Snapshot& snapshot, uint8_t temporaryAvailableMask = 0) {
    // Intentional sleep removes a player from the live connection mask, but it
    // must not remove that player from the frozen turn order. An unexpected
    // disconnect has no sleeping bit and is still skipped.
    const uint8_t eligibleMask = static_cast<uint8_t>(
        snapshot.rosterMask &
        (snapshot.connectedMask | snapshot.localControlMask |
         snapshot.sleepingMask | temporaryAvailableMask));
    return nextPlayerInMask(current, eligibleMask, snapshot.turnOrder);
}

inline bool start(Snapshot& snapshot, uint8_t turnOrder = kDefaultTurnOrder) {
    const uint8_t participants = lobbyParticipantMask(snapshot);
    if (snapshot.started || participants == 0 || !isTurnOrder(turnOrder)) {
        return false;
    }
    snapshot.started = true;
    snapshot.rosterMask = participants;
    snapshot.turnOrder = turnOrder;
    snapshot.completedTurns.fill(0);
    // OFF peers may still be linked for a few milliseconds while the BLE
    // owner drains the frozen-roster disconnect. Remove them from gameplay
    // state immediately so every replicated snapshot is internally valid.
    snapshot.connectedMask = static_cast<uint8_t>(
        snapshot.connectedMask & participants);
    snapshot.sleepingMask = static_cast<uint8_t>(
        snapshot.sleepingMask & participants);
    snapshot.turn = nextPlayerInMask(Player::None, participants, turnOrder);
    ++snapshot.version;
    return true;
}

inline bool setLobbyMode(
    Snapshot& snapshot, Player player, LobbyMode mode) {
    if (snapshot.started || !isPlayer(player)) {
        return false;
    }
    const uint8_t bit = playerBit(player);
    uint8_t nextEnabledMask = snapshot.lobbyEnabledMask;
    uint8_t nextLocalMask = snapshot.localControlMask;
    const uint8_t nextExplicitMask = static_cast<uint8_t>(
        snapshot.lobbyExplicitMask | bit);
    if (mode == LobbyMode::Local) {
        nextEnabledMask = static_cast<uint8_t>(nextEnabledMask | bit);
        nextLocalMask = static_cast<uint8_t>(nextLocalMask | bit);
    } else if (mode == LobbyMode::Physical) {
        nextEnabledMask = static_cast<uint8_t>(nextEnabledMask | bit);
        nextLocalMask = static_cast<uint8_t>(nextLocalMask & ~bit);
    } else {
        nextEnabledMask = static_cast<uint8_t>(nextEnabledMask & ~bit);
        nextLocalMask = static_cast<uint8_t>(nextLocalMask & ~bit);
    }
    if (nextEnabledMask == snapshot.lobbyEnabledMask &&
        nextLocalMask == snapshot.localControlMask &&
        nextExplicitMask == snapshot.lobbyExplicitMask) {
        return false;
    }
    snapshot.lobbyEnabledMask = nextEnabledMask;
    snapshot.localControlMask = nextLocalMask;
    snapshot.lobbyExplicitMask = nextExplicitMask;
    ++snapshot.version;
    return true;
}

inline bool setLocalControl(Snapshot& snapshot, Player player, bool enabled) {
    return setLobbyMode(
        snapshot, player, enabled ? LobbyMode::Local : LobbyMode::Physical);
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
    snapshot.turnOrder = kDefaultTurnOrder;
    snapshot.completedTurns.fill(0);
    snapshot.started = false;
    ++snapshot.version;
    return true;
}

inline bool connect(Snapshot& snapshot, Player player) {
    if (!isPlayer(player) || isConnected(snapshot, player) ||
        (snapshot.started && (snapshot.rosterMask & (1u << playerIndex(player))) == 0)) {
        return false;
    }
    const uint8_t bit = playerBit(player);
    snapshot.connectedMask = static_cast<uint8_t>(
        snapshot.connectedMask | bit);
    if (!snapshot.started && (snapshot.lobbyExplicitMask & bit) == 0) {
        snapshot.lobbyEnabledMask = static_cast<uint8_t>(
            snapshot.lobbyEnabledMask | bit);
        snapshot.localControlMask = static_cast<uint8_t>(
            snapshot.localControlMask & ~bit);
    }
    ++snapshot.version;
    return true;
}

inline bool disconnect(Snapshot& snapshot, Player player) {
    if (!isPlayer(player) || !isConnected(snapshot, player)) {
        return false;
    }
    const uint8_t bit = playerBit(player);
    snapshot.connectedMask = static_cast<uint8_t>(
        snapshot.connectedMask & ~bit);
    if (!snapshot.started && (snapshot.lobbyExplicitMask & bit) == 0) {
        snapshot.lobbyEnabledMask = static_cast<uint8_t>(
            snapshot.lobbyEnabledMask & ~bit);
        snapshot.localControlMask = static_cast<uint8_t>(
            snapshot.localControlMask & ~bit);
    }
    ++snapshot.version;
    return true;
}

// Presence hints are ephemeral and do not create a replicated revision. A
// later disconnect is the versioned transition; intentional sleep keeps the
// player eligible for the frozen turn order.
inline bool markSleeping(Snapshot& snapshot, Player player) {
    const uint8_t bit = playerBit(player);
    if (!isConnected(snapshot, player) || (snapshot.sleepingMask & bit) != 0) {
        return false;
    }
    snapshot.sleepingMask = static_cast<uint8_t>(snapshot.sleepingMask | bit);
    return true;
}

inline bool clearSleeping(Snapshot& snapshot, Player player) {
    const uint8_t bit = playerBit(player);
    const bool changed = (snapshot.sleepingMask & bit) != 0;
    snapshot.sleepingMask = static_cast<uint8_t>(snapshot.sleepingMask & ~bit);
    return changed;
}

// Authority startup never restores live links. Explicit lobby choices and the
// frozen roster survive. Scheduling and durable storage belong to the caller.
inline void resumeAuthority(Snapshot& snapshot, uint32_t& term) {
    snapshot.connectedMask = 0;
    if (!snapshot.started) {
        snapshot.lobbyEnabledMask &= snapshot.lobbyExplicitMask;
        snapshot.localControlMask &= snapshot.lobbyExplicitMask;
    }
    ++term;
}

inline void finishTurn(Snapshot& snapshot, Player player, uint8_t temporaryAvailableMask = 0) {
    ++snapshot.completedTurns[playerIndex(player)];
    snapshot.turn = nextTurnParticipant(player, snapshot, temporaryAvailableMask);
}

inline ApplyResult apply(Snapshot& snapshot, const ScoreAction& action) {
    if (!snapshot.started) {
        return ApplyResult::GameNotStarted;
    }
    if (!isPlayer(action.player) || action.operationId == 0) {
        return ApplyResult::UnknownPlayer;
    }
    const size_t index = playerIndex(action.player);
    if (!isConnected(snapshot, action.player) ||
        (snapshot.rosterMask & playerBit(action.player)) == 0) {
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
        finishTurn(snapshot, action.player);
    }
    ++snapshot.version;
    return action.passesTurn && !turnMayChange
               ? ApplyResult::AcceptedWithoutTurnChange
               : ApplyResult::Accepted;
}

inline ApplyResult applyLocal(Snapshot& snapshot, const LocalScoreAction& action) {
    if (!snapshot.started) {
        return ApplyResult::GameNotStarted;
    }
    if (!isPlayer(action.player)) {
        return ApplyResult::UnknownPlayer;
    }
    const uint8_t bit = playerBit(action.player);
    if ((snapshot.rosterMask & bit) == 0 || !isLocallyControllable(snapshot, action.player)) {
        return ApplyResult::NotConnected;
    }
    const size_t index = playerIndex(action.player);
    const int64_t nextScore = static_cast<int64_t>(snapshot.scores[index]) + action.scoreDelta;
    if (nextScore < std::numeric_limits<int32_t>::min() ||
        nextScore > std::numeric_limits<int32_t>::max()) {
        return ApplyResult::ScoreOutOfRange;
    }
    snapshot.scores[index] = static_cast<int32_t>(nextScore);
    const bool turnMayChange = action.passesTurn && snapshot.turn == action.player;
    if (turnMayChange) {
        // A disconnected rescue target is temporarily available for this
        // transition so a one-player game wraps back to the same color.
        finishTurn(snapshot, action.player, bit);
    }
    ++snapshot.version;
    return action.passesTurn && !turnMayChange
               ? ApplyResult::AcceptedWithoutTurnChange
               : ApplyResult::Accepted;
}

}  // namespace scorebot
