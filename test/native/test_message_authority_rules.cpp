#include <MessageAuthorityRules.hpp>
#include <cassert>
int main() {
    using scorebot::mayAcceptLeaderMessage;
    assert(mayAcceptLeaderMessage(false, 42, 42, 42));
    assert(!mayAcceptLeaderMessage(true, 42, 42, 42));
    assert(!mayAcceptLeaderMessage(false, 42, 41, 42));
    assert(!mayAcceptLeaderMessage(false, 42, 42, 41));
    assert(!mayAcceptLeaderMessage(false, 0, 0, 0));
}
