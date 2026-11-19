#include <BoardIdentity.hpp>
#include <PeerAdmissionRules.hpp>

#include <cassert>
#include <iostream>

namespace {

struct Candidate {
    uint32_t nodeId;
    uint64_t bluetoothMac;
    uint8_t offset{1};
    bool publicAddress{true};
    uint16_t protocol{scorebot::kWireProtocolVersion};
    uint32_t localPeerId{getNodeIdForRole(BoardRole::Leader)};

    scorebot::PeerAdmission classify() const {
        return scorebot::classifyPeerAdmission(
            nodeId, protocol, publicAddress, bluetoothMac, offset, localPeerId);
    }
};

void assertRejected(const Candidate& candidate) {
    const auto admission = candidate.classify();
    assert(admission.kind == scorebot::PeerKind::Rejected);
    assert(!admission.eligibleForFrozenRoster(0xff));
}

}  // namespace

int main() {
    using scorebot::PeerKind;

    constexpr BoardRole players[] = {
        BoardRole::Player_Red, BoardRole::Player_Blue,
        BoardRole::Player_Green, BoardRole::Player_White,
    };
    uint8_t expectedBit = 1;
    for (const auto role : players) {
        const auto& config = getRoleConfig(getNodeIdForRole(role));
        for (const uint8_t offset : {1, 2}) {
            Candidate player{config.nodeId, config.baseMac + offset, offset};
            const auto admission = player.classify();
            assert(admission.kind == PeerKind::Player);
            assert(admission.playerMask == expectedBit);
            // Every subset admits exactly its configured players. Reusing the
            // decision with a new mask models the insertion-time recheck.
            for (uint8_t mask = 0; mask < 16; ++mask) {
                assert(admission.eligibleForFrozenRoster(mask) ==
                       ((mask & expectedBit) != 0));
            }
            assert(!admission.eligibleForFrozenRoster(0xf0));

            player.offset = offset == 1 ? 2 : 1;
            assertRejected(player);  // Players require the configured MAC offset.
            player.offset = offset;
            player.localPeerId = player.nodeId;
            assertRejected(player);
        }
        expectedBit <<= 1;
    }
    assert(scorebot::roleMask(BoardRole::Unknown) == 0);
    assert(scorebot::roleMask(BoardRole::Leader) == 0);

    constexpr uint64_t displayMac = 0x112233445566ULL;
    for (const uint8_t identityOffset : {1, 2}) {
        for (const uint8_t localOffset : {1, 2}) {
            Candidate display{
                scorebot::boardIdFromBluetoothMac(displayMac, identityOffset),
                displayMac, localOffset};
            const auto admission = display.classify();
            assert(admission.kind == PeerKind::Display);
            assert(admission.playerMask == 0);
            for (uint8_t mask = 0; mask < 16; ++mask) {
                assert(admission.eligibleForFrozenRoster(mask));
            }
            display.localPeerId = display.nodeId;
            assertRejected(display);
        }
    }

    const auto& red = getRoleConfig(getNodeIdForRole(BoardRole::Player_Red));
    const Candidate player{red.nodeId, red.baseMac + 1};
    const Candidate display{
        scorebot::boardIdFromBluetoothMac(displayMac, 1), displayMac};
    for (const auto valid : {player, display}) {
        for (const uint16_t protocol : {0, scorebot::kWireProtocolVersion - 1,
                                       scorebot::kWireProtocolVersion + 1}) {
            auto candidate = valid;
            candidate.protocol = protocol;
            assertRejected(candidate);
        }
        auto candidate = valid;
        candidate.publicAddress = false;
        assertRejected(candidate);
        candidate = valid;
        candidate.nodeId = 0;
        assertRejected(candidate);
    }

    auto candidate = player;
    candidate.nodeId = getNodeIdForRole(BoardRole::Player_Blue);
    assertRejected(candidate);  // Another configured identity cannot claim red's MAC.
    candidate = player;
    candidate.bluetoothMac = 0x112200000000ULL | (player.bluetoothMac & 0xffffffffULL);
    assertRejected(candidate);  // Player validation uses the full MAC, not its tail.
    candidate = display;
    candidate.nodeId += 9;
    assertRejected(candidate);
    candidate = display;
    candidate.nodeId = red.nodeId;
    assertRejected(candidate);

    // A configured leaderboard is neither a player nor a passive display,
    // even when it is not the local peer.
    const auto& leader = getRoleConfig(getNodeIdForRole(BoardRole::Leader));
    for (const uint8_t offset : {1, 2}) {
        candidate = {leader.nodeId, leader.baseMac + offset, offset};
        candidate.localPeerId = red.nodeId;
        assertRejected(candidate);
    }

    // Preserve the display rule's existing boundary: an unknown identity
    // matching the alternate offset is a display even on a configured MAC.
    candidate = player;
    candidate.nodeId = scorebot::boardIdFromBluetoothMac(player.bluetoothMac, 2);
    assert(getRoleConfig(candidate.nodeId).role == BoardRole::Unknown);
    assert(candidate.classify().kind == PeerKind::Display);

    std::cout << "Peer-admission tests passed\n";
}
