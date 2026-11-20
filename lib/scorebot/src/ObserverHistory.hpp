#pragma once
#include <LeaderboardBroadcast.hpp>

namespace scorebot {
struct ObserverReceptionClock {
    uint32_t lastSeen{0}, gaps{0};
    bool received{false};
    void receive(uint32_t now) {
        if (received && uint32_t(now - lastSeen) >= 12000) ++gaps;
        lastSeen = now;
        received = true;
    }
};
// Measurements of received snapshots, never an inferred game clock/event log.
struct ObserverChange {
    Player player{Player::None};
    int32_t score{0};
    int64_t delta{0};
    bool operator==(const ObserverChange& other) const {
        return player == other.player && score == other.score && delta == other.delta;
    }
};
struct ObserverHistory {
    LeaderboardBroadcast snapshot{};
    std::array<ObserverChange, 2> changes{};
    uint32_t firstSeen{0}, lastSeen{0}, snapshots{0}, gaps{0};
    unsigned changeCount{0};
    int rssi{0};
    bool received{false};

    void observe(const LeaderboardBroadcast& next, uint32_t now, int signal, uint32_t receptionGaps = 0) {
        if (!received) firstSeen = now;
        gaps = receptionGaps;
        const bool changed = !received || next.scores != snapshot.scores ||
            next.rosterMask != snapshot.rosterMask || next.turn != snapshot.turn;
        if (received) {
            for (size_t i = 0; i < kPlayerCount; ++i) {
                if (next.scores[i] == snapshot.scores[i]) continue;
                changes[1] = changes[0];
                changes[0] = {static_cast<Player>(i), next.scores[i],
                              int64_t(next.scores[i]) - snapshot.scores[i]};
                if (changeCount < changes.size()) ++changeCount;
            }
        }
        if (changed) ++snapshots;
        snapshot = next;
        lastSeen = now;
        rssi = signal;
        received = true;
    }
};
}  // namespace scorebot
