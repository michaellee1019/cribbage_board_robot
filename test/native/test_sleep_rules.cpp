#include <SleepRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using namespace scorebot::sleep;
    static_assert(kPlayerIdleTimeoutMs == 5u * 60u * 1000u);
    static_assert(kLeaderboardIdleTimeoutMs == 10u * 60u * 1000u);
    static_assert(kPendingScoreIdleTimeoutMs == 20u * 60u * 1000u);
    assert(idleTimeoutMs(false, false) == kPlayerIdleTimeoutMs);
    assert(idleTimeoutMs(false, true) == kPendingScoreIdleTimeoutMs);
    assert(idleTimeoutMs(true, false) == kLeaderboardIdleTimeoutMs);
    assert(idleTimeoutMs(true, true) == kLeaderboardIdleTimeoutMs);

    assert(!isDue(kPlayerIdleTimeoutMs - 1, 0, 0, false,
                  kPlayerIdleTimeoutMs));
    assert(isDue(kPlayerIdleTimeoutMs, 0, 0, false,
                 kPlayerIdleTimeoutMs));
    assert(!isDue(1000 + kLeaderboardIdleTimeoutMs - 1, 1000, 0, false,
                  kLeaderboardIdleTimeoutMs));
    assert(isDue(1000 + kLeaderboardIdleTimeoutMs, 1000, 0, false,
                 kLeaderboardIdleTimeoutMs));
    assert(!isDue(500 + kPlayerIdleTimeoutMs, 100, 500, true,
                  kPlayerIdleTimeoutMs));
    assert(isDue(500 + kPlayerIdleTimeoutMs, 100, 500, false,
                 kPlayerIdleTimeoutMs));
    assert(isDue(20, UINT32_MAX - kPlayerIdleTimeoutMs + 20, 0, false,
                 kPlayerIdleTimeoutMs));

    assert(!showSavedScore(0));
    assert(!showSavedScore(kRejoinAlternateMs - 1));
    assert(showSavedScore(kRejoinAlternateMs));
    assert(!showSavedScore(kRejoinAlternateMs * 2));
    std::cout << "Sleep-rule tests passed\n";
}
