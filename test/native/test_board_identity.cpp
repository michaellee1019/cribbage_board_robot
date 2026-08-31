#if !defined(ARDUINO)

#include <BoardIdentity.hpp>
#include <BoardRole.hpp>

#include <cassert>
#include <iostream>

int main() {
    // USB MAC 98:3d:ae:ea:17:bc is the leaderboard's historical ID.
    assert(scorebot::boardIdFromEfuseMac(0x0000bc17eaae3d98ULL) == 0xaeea17bc);
    // USB MAC 98:3d:ae:ea:0f:40 is the red player's historical ID.
    assert(scorebot::boardIdFromEfuseMac(0x0000400feaae3d98ULL) == 0xaeea0f40);
    // ESP32-S3 Arduino currently allocates two universal MACs, so Bluetooth
    // uses base + 1. The full address is allowlisted, not just its node-ID tail.
    assert(getRoleConfigForBluetoothMac(0x983daeea17bdULL, 1).role == BoardRole::Leader);
    assert(getRoleConfigForBluetoothMac(0x983daeea0f41ULL, 1).role == BoardRole::Player_Red);
    assert(getRoleConfigForBluetoothMac(0x64e83350c4b9ULL, 1).role == BoardRole::Player_Blue);
    assert(getRoleConfigForBluetoothMac(0x983daeea17beULL, 1).role == BoardRole::Unknown);

    constexpr BoardRole roles[] = {
        BoardRole::Player_Red, BoardRole::Player_Blue, BoardRole::Player_Green,
        BoardRole::Player_White, BoardRole::Leader,
    };
    for (const BoardRole role : roles) {
        const BoardRoleConfig& config = getRoleConfig(getNodeIdForRole(role));
        assert(static_cast<uint32_t>(config.baseMac) == config.nodeId);
    }
    std::cout << "Board-identity tests passed\n";
}

#endif
