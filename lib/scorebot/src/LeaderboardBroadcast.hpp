#pragma once

#include <GameRules.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace scorebot {

struct LeaderboardBroadcast {
    std::array<int32_t, kPlayerCount> scores{};
    uint8_t rosterMask{0};
    Player turn{Player::None};
};

// Experimental manufacturer ID, Scorebot leaderboard tag, format version.
// This independent, read-only packet fits in one legacy BLE advertisement.
constexpr std::array<uint8_t, 6> kLeaderboardHeader{0xff, 0xff, 'S', 'B', 'L', 1};
using LeaderboardPacket = std::array<uint8_t, kLeaderboardHeader.size() + 2 + 4 * kPlayerCount>;
static_assert(LeaderboardPacket{}.size() + 2 <= 31);

inline LeaderboardPacket encodeLeaderboard(const LeaderboardBroadcast& state) {
    LeaderboardPacket packet{};
    auto out = std::copy(kLeaderboardHeader.begin(), kLeaderboardHeader.end(), packet.begin());
    *out++ = state.rosterMask;
    *out++ = static_cast<uint8_t>(state.turn);
    for (int32_t score : state.scores) {
        uint32_t bits = static_cast<uint32_t>(score);
        for (size_t byte = 0; byte < 4; ++byte, bits >>= 8) {
            *out++ = static_cast<uint8_t>(bits);
        }
    }
    return packet;
}

inline bool decodeLeaderboard(const uint8_t* data, size_t size, LeaderboardBroadcast& state) {
    if (size != LeaderboardPacket{}.size() ||
        !std::equal(kLeaderboardHeader.begin(), kLeaderboardHeader.end(), data)) {
        return false;
    }
    const uint8_t* in = data + kLeaderboardHeader.size();
    const uint8_t roster = *in++;
    const auto turn = static_cast<Player>(*in++);
    constexpr uint8_t allPlayers = (1u << kPlayerCount) - 1;
    if ((roster & ~allPlayers) ||
        (turn != Player::None && (roster & playerBit(turn)) == 0)) {
        return false;
    }
    state.rosterMask = roster;
    state.turn = turn;
    for (int32_t& score : state.scores) {
        uint32_t bits = 0;
        for (size_t byte = 0; byte < 4; ++byte) {
            bits |= static_cast<uint32_t>(*in++) << (8 * byte);
        }
        score = static_cast<int32_t>(bits & 0x80000000u
                                        ? static_cast<int64_t>(bits) - 0x100000000LL
                                        : bits);
    }
    return true;
}

}  // namespace scorebot
