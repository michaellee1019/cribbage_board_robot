#include <SleepRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using namespace scorebot::sleep;
    static_assert(kUnpairedPlayerIdleTimeoutMs == 5u * 60u * 1000u);
    static_assert(kSessionIdleTimeoutMs == 10u * 60u * 1000u);
    static_assert(kPendingScoreIdleTimeoutMs == 20u * 60u * 1000u);
    assert(offlineIdleTimeoutMs(false, false) == kUnpairedPlayerIdleTimeoutMs);
    assert(offlineIdleTimeoutMs(false, true) == kPendingScoreIdleTimeoutMs);
    assert(offlineIdleTimeoutMs(true, false) == kSessionIdleTimeoutMs);
    assert(offlineIdleTimeoutMs(true, true) == kPendingScoreIdleTimeoutMs);

    constexpr uint32_t lastInput = 1000;
    const uint32_t deadline = lastInput + kSessionIdleTimeoutMs;
    SleepHandoff handoff;
    assert(!handoff.grantDue(deadline - 1,
                            elapsed(deadline - 1, lastInput, kSessionIdleTimeoutMs)));
    assert(handoff.grantDue(deadline, elapsed(deadline, lastInput, kSessionIdleTimeoutMs)));
    handoff.recordGrant(deadline);
    assert(!handoff.grantDue(deadline + kGrantRepeatMs - 1, true));
    assert(handoff.grantDue(deadline + kGrantRepeatMs, true));
    handoff.recordGrant(deadline + kGrantRepeatMs);
    assert(!handoff.allowsSleep(deadline + kAuthorityGraceMs - 1));
    assert(handoff.allowsSleep(deadline + kAuthorityGraceMs));

    SleepGrant grant;
    assert(!grant.allowsSleep(deadline));
    grant.observe(deadline, true);
    assert(!grant.allowsSleep(deadline + kFollowerGraceMs - 1));
    assert(!grant.allowsSleep(deadline + kFollowerGraceMs));
    grant.observe(deadline + kGrantRepeatMs, true);
    assert(grant.allowsSleep(deadline + kGrantRepeatMs));
    assert(grant.allowsSleep(deadline + kGrantLifetimeMs));
    assert(!grant.allowsSleep(deadline + kGrantRepeatMs + kGrantLifetimeMs));
    grant.observe(deadline + kGrantRepeatMs + kGrantLifetimeMs, true);
    assert(!grant.allowsSleep(deadline + kGrantRepeatMs + kGrantLifetimeMs));
    grant.observe(deadline + 3000, false);
    assert(!grant.allowsSleep(deadline + 3000));

    constexpr uint32_t beforeWrap = UINT32_MAX - 10;
    grant.observe(beforeWrap, true);
    grant.observe(beforeWrap + kGrantRepeatMs, true);
    assert(grant.allowsSleep(beforeWrap + kGrantRepeatMs));
    grant.observe(0, false);
    grant.observe(0, true);
    grant.observe(kGrantRepeatMs, true);
    assert(grant.allowsSleep(kGrantRepeatMs));
    assert(elapsed(20, UINT32_MAX - kSessionIdleTimeoutMs + 20, kSessionIdleTimeoutMs));

    std::cout << "Sleep-rule tests passed\n";
}
