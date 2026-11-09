#include <BoardIdentity.hpp>
#include <BoardRole.hpp>
#include <DisplayPeerRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    assert(scorebot::boardIdFromBluetoothMac(0x983daeea17bdULL, 1) == 0xaeea17bc);
    assert(scorebot::boardIdFromEfuseMac(0x0000bc17eaae3d98ULL) ==
           scorebot::boardIdFromBluetoothMac(0x983daeea17bdULL, 1));

    const uint64_t wallMac = 0x112233445566ULL;
    const uint32_t wallOffset1 = scorebot::boardIdFromBluetoothMac(wallMac, 1);
    const uint32_t wallOffset2 = scorebot::boardIdFromBluetoothMac(wallMac, 2);
    assert(scorebot::isDisplaySubscriber(wallOffset1, wallMac, BoardRole::Unknown));
    assert(scorebot::isDisplaySubscriber(wallOffset2, wallMac, BoardRole::Unknown));
    assert(!scorebot::isDisplaySubscriber(wallOffset1 + 9, wallMac, BoardRole::Unknown));
    assert(!scorebot::isDisplaySubscriber(0, wallMac, BoardRole::Unknown));
    assert(!scorebot::isDisplaySubscriber(
        0xaeea17bc, 0x983daeea17bdULL, BoardRole::Leader));
    assert(!scorebot::isDisplaySubscriber(
        0xaeea0f40, 0x983daeea0f41ULL, BoardRole::Player_Red));

    std::cout << "Display-peer tests passed\n";
    return 0;
}
