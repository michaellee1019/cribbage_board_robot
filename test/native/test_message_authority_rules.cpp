#include <MessageAuthorityRules.hpp>

#include <cassert>

int main() {
    using scorebot::mayAcceptSnapshot;

    assert(mayAcceptSnapshot(BoardRole::Player_Red, BoardRole::Leader));
    assert(mayAcceptSnapshot(BoardRole::Player_Blue, BoardRole::Leader));
    assert(!mayAcceptSnapshot(BoardRole::Leader, BoardRole::Player_Red));
    assert(!mayAcceptSnapshot(BoardRole::Leader, BoardRole::Leader));
    assert(!mayAcceptSnapshot(BoardRole::Player_Red, BoardRole::Player_Blue));
    assert(!mayAcceptSnapshot(BoardRole::Unknown, BoardRole::Leader));
}
