#include <VisualFeedbackRules.hpp>

#include <cassert>
#include <iostream>
#include <string_view>

int main() {
    using scorebot::turnPulseAt;

    assert(turnPulseAt(0).segmentBrightness == 0);
    assert(turnPulseAt(0).lightLevel == 32);
    assert(turnPulseAt(999).segmentBrightness == 14);
    assert(turnPulseAt(1000).segmentBrightness == 15);
    assert(turnPulseAt(1000).lightLevel == scorebot::kTurnLightMinLevel);
    assert(turnPulseAt(1999).segmentBrightness == 15);
    assert(turnPulseAt(2000).segmentBrightness == 15);
    assert(turnPulseAt(2500).segmentBrightness == 7);
    assert(turnPulseAt(2999).segmentBrightness == 0);
    assert(turnPulseAt(3000).segmentBrightness == 0);
    for (uint32_t elapsed = 0; elapsed < scorebot::kTurnCycleMs; ++elapsed) {
        assert(turnPulseAt(elapsed).lightLevel >= scorebot::kTurnLightMinLevel);
    }

    assert(scorebot::leaderboardLightLevel(0) == 2);
    assert(scorebot::leaderboardLightLevel(15) == 32);
    assert(scorebot::leaderboardLightLevel(255) == 32);
    assert(!scorebot::turnStatusShowsScore(0));
    assert(!scorebot::turnStatusShowsScore(999));
    assert(scorebot::turnStatusShowsScore(1000));
    assert(scorebot::turnStatusShowsScore(1999));
    assert(!scorebot::turnStatusShowsScore(2000));
    using scorebot::ScoreDisplayStyle;
    const auto plainScore = [](int32_t score, uint32_t frame) {
        return scorebot::scoreDisplayFrame(score, frame, ScoreDisplayStyle::Plain);
    };
    assert(std::string_view(plainScore(160, 0).data()) == "160 ");
    assert(plainScore(160, 0) == plainScore(160, 1));
    assert(std::string_view(plainScore(-42, 3).data()) == "-42 ");
    assert(std::string_view(plainScore(1234, 5).data()) == "1234");
    assert(std::string_view(plainScore(12345, 1).data()) == "2345");
    const auto activeScore = [](uint32_t elapsed) {
        return scorebot::activeTurnFrame(160, elapsed, ScoreDisplayStyle::Plain);
    };
    assert(std::string_view(activeScore(0).data()) == "BEEF");
    assert(std::string_view(activeScore(1000).data()) == "160 ");
    assert(activeScore(1000) == activeScore(3000));
    assert(scorebot::invalidGestureLightOn(0));
    assert(scorebot::invalidGestureLightOn(scorebot::kInvalidGestureLightMs - 1));
    assert(!scorebot::invalidGestureLightOn(scorebot::kInvalidGestureLightMs));
    std::cout << "Visual-feedback tests passed\n";
}
