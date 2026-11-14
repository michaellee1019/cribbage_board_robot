#include <Event.hpp>
#include <LeaderboardBroadcast.hpp>
#include <Protocol.hpp>

#include <cassert>
#include <iostream>
#include <limits>

int main() {
    static_assert(scorebot::kMaxWireMessageSize >= 300);
    static_assert(sizeof(MessageReceivedEvent::message) ==
                  scorebot::kMaxWireMessageSize + 1);
    assert(scorebot::kWireProtocolVersion == 4);
    {
        using namespace scorebot;
        const LeaderboardBroadcast scores{
            {std::numeric_limits<int32_t>::min(), 21, -5, std::numeric_limits<int32_t>::max()},
            0x0f, Player::White};
        auto packet = encodeLeaderboard(scores);
        assert(packet[8] == 0 && packet[11] == 0x80 && packet[12] == 21);

        LeaderboardBroadcast decoded{};
        assert(decodeLeaderboard(packet.data(), packet.size(), decoded));
        assert(decoded.scores == scores.scores);
        assert(decoded.rosterMask == scores.rosterMask);
        assert(decoded.turn == scores.turn);
        assert(!decodeLeaderboard(packet.data(), packet.size() - 1, decoded));

        packet[6] = playerBit(Player::Red);
        assert(!decodeLeaderboard(packet.data(), packet.size(), decoded));
        ++packet[5];
        assert(!decodeLeaderboard(packet.data(), packet.size(), decoded));

        const auto lobby = encodeLeaderboard({});
        assert(decodeLeaderboard(lobby.data(), lobby.size(), decoded));
        assert(decoded.rosterMask == 0 && decoded.turn == Player::None);
    }
    std::cout << "Wire-protocol tests passed\n";
    return 0;
}
