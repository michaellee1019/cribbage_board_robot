#include <Event.hpp>
#include <Protocol.hpp>

#include <cassert>
#include <iostream>

int main() {
    static_assert(scorebot::kMaxWireMessageSize >= 276);
    static_assert(sizeof(MessageReceivedEvent::message) ==
                  scorebot::kMaxWireMessageSize + 1);
    assert(scorebot::kWireProtocolVersion != 0);
    std::cout << "Wire-protocol tests passed\n";
    return 0;
}
