#include <PairingRules.hpp>
#include <cassert>
using namespace scorebot;
int main() {
    Discovery host{12, 12, 34, 0, DeviceKind::Leaderboard, BoardRole::Leader, Authority | Selected, 0};
    Discovery display{56, 12, 34, 0, DeviceKind::Display, BoardRole::Unknown, Selected, 0};
    assert(classifyPeerAdmission(host, display).kind == PeerKind::Display);
    assert(classifyPeerAdmission(host, display).eligibleForFrozenRoster(0));
    display.role = BoardRole::Player_Red;
    assert(classifyPeerAdmission(host, display).kind == PeerKind::Rejected);
    display.kind = DeviceKind::Leaderboard; display.role = BoardRole::Leader;
    assert(classifyPeerAdmission(host, display).kind == PeerKind::Controller);
    display.flags |= Authority;
    assert(classifyPeerAdmission(host, display).kind == PeerKind::Rejected);
}
