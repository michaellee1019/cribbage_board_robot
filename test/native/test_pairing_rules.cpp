#include <PairingRules.hpp>
#include <cassert>
#include <iostream>
using namespace scorebot;

int main() {
    assert(validDisplayCount(1) && validDisplayCount(4));
    for (uint8_t count : {0, 2, 3, 5}) assert(!validDisplayCount(count));
    assert(kindForDisplayCount(4) == DeviceKind::Leaderboard);
    assert(kindForDisplayCount(1) == DeviceKind::Player);
    Discovery leader{0x12345678, 0x12345678, 0xabcdef01, 0, DeviceKind::Leaderboard,
                     BoardRole::Leader, Authority | Selected, 15};
    for (uint8_t pairing : {0, 20}) {
        leader.pairing = pairing;
        auto bytes = encodeDiscovery(leader);
        static_assert(DiscoveryPacket{}.size() + 2 + 3 <= 31);
        assert(bytes[5] == 0x78 && bytes[8] == 0x12);
        Discovery decoded{};
        assert(decodeDiscovery(bytes.data(), bytes.size(), decoded));
        assert(sameSession(decoded, leader) && decoded.node == leader.node && decoded.occupied == 15);
        const Discovery before = decoded;
        assert(!decodeDiscovery(nullptr, bytes.size(), decoded));
        assert(!decodeDiscovery(bytes.data(), bytes.size() - 1, decoded));
        bytes[4]--;
        assert(!decodeDiscovery(bytes.data(), bytes.size(), decoded));
        assert(decoded.session == before.session);
    }
    auto wrong = leader;
    wrong.session++;
    assert(!sameSession(leader, wrong)); // Same displayed number is not identity.
    wrong = leader; wrong.leader++;
    assert(!sameSession(leader, wrong));
    wrong = leader; wrong.pairing = 21;
    assert(!validDiscovery(wrong));
    wrong = leader; wrong.node = 0;
    assert(!validDiscovery(wrong));
    wrong = leader; wrong.kind = DeviceKind::Player;
    assert(!validDiscovery(wrong));
    for (uint32_t random = 0; random < 300; ++random) {
        const auto id = choosePairingId(random, 0);
        assert(id <= 20);
        assert(choosePairingId(random, 1u << id) != id);
        assert(choosePairingId(random, (1u << 21) - 1) == kNoPairingId);
        assert(choosePairingId(random, ((1u << 21) - 1) ^ (1u << 20)) == 20);
    }
    auto small = leader; small.flags = 0; small.node = small.leader = 10;
    auto large = small; large.node = large.leader = 11;
    assert(preferredStartupLeader(small, large));
    assert(!preferredStartupLeader(large, small));
    large.flags = Authority | Selected;
    assert(preferredStartupLeader(large, small));
    small.flags = Recovering;
    assert(!preferredStartupLeader(small, large));
    large.flags = 0;
    assert(preferredStartupLeader(small, large));
    auto follower = large; follower.flags = Selected; follower.leader = small.node;
    assert(!preferredStartupLeader(follower, large));
    std::cout << "Pairing packet, hardware, collision, and startup-election tests passed\n";
}
