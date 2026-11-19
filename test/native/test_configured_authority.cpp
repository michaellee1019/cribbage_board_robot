#include <BoardRole.hpp>
#include <LeaderIdentity.hpp>
#include <MessageAuthorityRules.hpp>
#include <PeerAdmissionRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using namespace scorebot;
    const uint32_t id = kLeaderNodeId;
    const uint64_t mac = kExternalLeader ? kLeaderBluetoothMac : 0x983daeea17bdULL;
    assert(getNodeIdForRole(BoardRole::Leader) == id);
    assert(getRoleConfig(id).role == BoardRole::Leader);
    assert(isConfiguredLeaderPeer(id, kWireProtocolVersion, true, mac, 1));
    if (kExternalLeader) {
        assert(isConfiguredLeaderPeer(id, kWireProtocolVersion, true, mac, 2));
        assert(getRoleConfig(2934577084u).role == BoardRole::Unknown);
        assert(trustedLeaderForBluetoothAddress(true, 0x983daeea17bdULL, 1) == 0);
        assert(trustedLeaderForBluetoothAddress(true, 0x983daeea17beULL, 2) == 0);
    } else {
        assert(isConfiguredLeaderPeer(id, kWireProtocolVersion, true, mac + 1, 2));
        assert(!isConfiguredLeaderPeer(id, kWireProtocolVersion, true, mac + 1, 1));
    }
    assert(!isConfiguredLeaderPeer(id, kWireProtocolVersion, false, mac, 1));
    assert(!isConfiguredLeaderPeer(id, kWireProtocolVersion - 1, true, mac, 1));
    assert(!isConfiguredLeaderPeer(id, kWireProtocolVersion + 1, true, mac, 1));
    assert(!isConfiguredLeaderPeer(id, kWireProtocolVersion, true, mac ^ (1ULL << 40), 1));
    assert(!isConfiguredLeaderPeer(id + 1, kWireProtocolVersion, true, mac, 1));
    assert(!isConfiguredLeaderPeer(0, kWireProtocolVersion, true, mac, 1));
    for (auto role : {BoardRole::Player_Red, BoardRole::Player_Blue, BoardRole::Player_Green, BoardRole::Player_White}) {
        const uint32_t player = getNodeIdForRole(role);
        assert(mayAcceptLeaderMessage(role, id, id));
        assert(!mayAcceptLeaderMessage(role, player, id));
        assert(!mayAcceptLeaderMessage(role, id, player));
        assert(!mayAcceptLeaderMessage(role, 0x11223344, id));
        const auto& config = getRoleConfig(player);
        assert(!isConfiguredLeaderPeer(id, kWireProtocolVersion, true, config.baseMac + 1, 1));
    }
    assert(!mayAcceptLeaderMessage(BoardRole::Leader, id, id));
    assert(!mayAcceptLeaderMessage(BoardRole::Unknown, id, id));
    // A second leaderboard cannot join the authority's player/display uplink.
    assert(classifyPeerAdmission(id, kWireProtocolVersion, true, mac, 1,
        getNodeIdForRole(BoardRole::Player_Red)).kind == PeerKind::Rejected);
    constexpr uint64_t displayMac = 0x112233445566ULL;
    const uint32_t display = boardIdFromBluetoothMac(displayMac, 1);
    assert(classifyPeerAdmission(display, kWireProtocolVersion, true, displayMac, 1, id).kind == PeerKind::Display);
    assert(!isConfiguredLeaderPeer(display, kWireProtocolVersion, true, displayMac, 1));
    assert(!mayAcceptLeaderMessage(BoardRole::Player_Red, display, id));
    std::cout << (kExternalLeader ? "Configured Pi" : "Default hardware") << " authority checks passed\n";
}
