#include <BoardIdentity.hpp>
#include <BoardRole.hpp>
#include <cassert>
int main() {
    assert(scorebot::boardIdFromEfuseMac(0x0000665544332211ULL) == 0x33445566u);
    assert(suggestedRole(0x33445566) == BoardRole::Unknown);
    assert(suggestedRole(2934574912u) == BoardRole::Player_Red);
    assert(suggestedRole(2934416992u) == BoardRole::Player_Yellow);
    static_assert(static_cast<unsigned>(BoardRole::Player_Yellow) == 5);
    static_assert(BoardRole::Player_White == BoardRole::Player_Yellow);
    // Styles work without a connected or preconfigured board of that color.
    assert(getRoleStyle(BoardRole::Player_Blue).color == 0x0000ff);
    assert(getRoleStyle(BoardRole::Player_Green).playerNumber == 3);
    assert(getRoleStyle(BoardRole::Player_Yellow).color == 0xffff00);
    assert(getRoleStyle(static_cast<BoardRole>(250)).role == BoardRole::Unknown);
}
