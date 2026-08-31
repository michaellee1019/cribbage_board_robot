#include <VisualFeedbackRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using scorebot::turnPulseAt;

    assert(turnPulseAt(0).segmentBrightness == 0);
    assert(turnPulseAt(0).lightLevel == 32);
    assert(turnPulseAt(999).segmentBrightness == 14);
    assert(turnPulseAt(1000).segmentBrightness == 15);
    assert(turnPulseAt(1000).lightLevel == 0);
    assert(turnPulseAt(1999).segmentBrightness == 15);
    assert(turnPulseAt(2000).segmentBrightness == 15);
    assert(turnPulseAt(2500).segmentBrightness == 7);
    assert(turnPulseAt(2999).segmentBrightness == 0);
    assert(turnPulseAt(3000).segmentBrightness == 0);

    assert(scorebot::leaderboardLightLevel(0) == 2);
    assert(scorebot::leaderboardLightLevel(15) == 32);
    assert(scorebot::leaderboardLightLevel(255) == 32);
    std::cout << "Visual-feedback tests passed\n";
}
