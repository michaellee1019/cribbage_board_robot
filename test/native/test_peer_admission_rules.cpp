#include <PeerAdmissionRules.hpp>
#include <cassert>
#include <iostream>
using namespace scorebot;
int main() {
    Discovery host{123, 123, 456, 20, DeviceKind::Leaderboard, BoardRole::Leader, Authority | Selected, 0};
    Discovery player{987, 123, 456, 20, DeviceKind::Player, BoardRole::Player_Red, Selected, 0};
    PeerDirectory peers;
    constexpr uint64_t radio = 0x112233445566ULL;
    assert(peers.admit(host, player, radio, 1).kind == PeerKind::Player); // Random address, arbitrary node.
    assert(peers.occupied() == 1);
    assert(peers.admit(host, player, radio, 1).kind == PeerKind::Player);
    assert(peers.admit(host, player, radio ^ (1ULL << 40), 1).kind == PeerKind::Rejected);
    assert(peers.admit(host, player, radio, 0).kind == PeerKind::Rejected);
    auto other = player; other.node++;
    assert(peers.admit(host, other, radio + 1, 0).kind == PeerKind::Rejected); // Occupied color.
    other.role = BoardRole::Player_Blue;
    assert(peers.admit(host, other, radio, 1).kind == PeerKind::Rejected); // One radio cannot claim two nodes.
    assert(peers.admit(host, other, radio + 1, 0).kind == PeerKind::Player);
    assert(peers.occupied() == 3);
    auto wrong = player; wrong.session++;
    assert(classifyPeerAdmission(host, wrong).kind == PeerKind::Rejected);
    wrong = player; wrong.leader++;
    assert(classifyPeerAdmission(host, wrong).kind == PeerKind::Rejected);
    wrong = player; wrong.pairing = 0;
    assert(classifyPeerAdmission(host, wrong).kind == PeerKind::Player); // Selector changes do not change identity.
    wrong = player; wrong.flags = 0;
    assert(classifyPeerAdmission(host, wrong).kind == PeerKind::Rejected);
    wrong = player; wrong.role = BoardRole::Unknown;
    assert(classifyPeerAdmission(host, wrong).kind == PeerKind::Rejected);
    wrong = player; wrong.node = host.node;
    assert(classifyPeerAdmission(host, wrong).kind == PeerKind::Rejected);
    assert(peers.admit(host, player, 0, 0).kind == PeerKind::Rejected);
    assert(peers.admit(host, player, 1ULL << 48, 0).kind == PeerKind::Rejected);
    assert(peers.admit(host, player, radio, 2).kind == PeerKind::Rejected);
    const auto admission = classifyPeerAdmission(host, player);
    assert(admission.eligibleForFrozenRoster(1));
    assert(!admission.eligibleForFrozenRoster(2));
    PeerDirectory independent;
    assert(independent.admit(host, player, radio + 10, 0).kind == PeerKind::Player);
    std::cout << "Runtime peer admission tests passed\n";
}
