#if !defined(ARDUINO)

#include <BoardIdentity.hpp>

#include <cassert>
#include <iostream>

int main() {
    // USB MAC 98:3d:ae:ea:17:bc is the leaderboard's historical ID.
    assert(scorebot::boardIdFromEfuseMac(0x0000bc17eaae3d98ULL) == 0xaeea17bc);
    // USB MAC 98:3d:ae:ea:0f:40 is the red player's historical ID.
    assert(scorebot::boardIdFromEfuseMac(0x0000400feaae3d98ULL) == 0xaeea0f40);
    std::cout << "Board-identity tests passed\n";
}

#endif
