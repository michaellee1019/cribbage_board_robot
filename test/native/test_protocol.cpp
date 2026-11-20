#include <Event.hpp>
#include <LeaderboardBroadcast.hpp>
#include <Protocol.hpp>
#include <ObserverHistory.hpp>

#include <cassert>
#include <iostream>
#include <limits>

int main() {
    static_assert(scorebot::kMaxWireMessageSize >= 300);
    static_assert(sizeof(MessageReceivedEvent::message) ==
                  scorebot::kMaxWireMessageSize + 1);
    assert(scorebot::kWireProtocolVersion == 5);
    {
        using namespace scorebot;
        const LeaderboardBroadcast scores{
            {std::numeric_limits<int32_t>::min(), 21, -5, std::numeric_limits<int32_t>::max()},
            0x0f, Player::Yellow};
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
    {
        using namespace scorebot;
        ObserverHistory history;
        LeaderboardBroadcast state{{10, 20, 0, 0}, 3, Player::Red};
        history.observe(state, 0xfffffff0u, -70);
        assert(history.changeCount == 0 && history.snapshots == 1);
        history.observe(state, 20, -71); // millis wrap, duplicate is not an action.
        assert(history.gaps == 0 && history.snapshots == 1);
        state.scores[0] = std::numeric_limits<int32_t>::min();
        history.observe(state, 13000, -80, 1);
        assert(history.gaps == 1 && history.changes[0].delta == int64_t(state.scores[0]) - 10);
        state.scores[0] = std::numeric_limits<int32_t>::max();
        history.observe(state, 14000, -75);
        assert(history.changeCount == 2 && history.changes[0].delta == 4294967295LL);
        assert(history.changes[1].score == std::numeric_limits<int32_t>::min());
        state.turn = Player::Blue;
        history.observe(state, 15000, -72);
        assert(history.snapshots == 4 && history.changeCount == 2);
        ObserverReceptionClock reception;
        reception.receive(0xfffffff0u);
        reception.receive(20);
        assert(reception.gaps == 0);
        for (uint32_t now = 1000; now <= 30000; now += 1000) reception.receive(now);
        // Renderer was blocked for 30s; callback still received throughout.
        history.observe(state, reception.lastSeen, -70, reception.gaps);
        assert(history.gaps == 0);
        reception.receive(45000);
        history.observe(state, reception.lastSeen, -70, reception.gaps);
        assert(history.gaps == 1);
    }
    std::cout << "Wire-protocol tests passed\n";
    return 0;
}
