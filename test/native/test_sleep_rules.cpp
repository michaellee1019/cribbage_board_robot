#include <SleepRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using namespace scorebot::sleep;
    assert(!isDue(kIdleTimeoutMs - 1, 0, 0, false));
    assert(isDue(kIdleTimeoutMs, 0, 0, false));
    assert(!isDue(1000 + kIdleTimeoutMs - 1, 1000, 0, false));
    assert(isDue(1000 + kIdleTimeoutMs, 1000, 0, false));
    assert(!isDue(500 + kIdleTimeoutMs, 100, 500, true));
    assert(isDue(500 + kIdleTimeoutMs, 100, 500, false));
    assert(isDue(20, UINT32_MAX - kIdleTimeoutMs + 20, 0, false));

    assert(!showSavedScore(0));
    assert(!showSavedScore(kRejoinAlternateMs - 1));
    assert(showSavedScore(kRejoinAlternateMs));
    assert(!showSavedScore(kRejoinAlternateMs * 2));
    std::cout << "Sleep-rule tests passed\n";
}
