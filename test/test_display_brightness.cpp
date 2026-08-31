#include <DisplayBrightness.hpp>

#include <cassert>
#include <iostream>
#include <limits>

using display_brightness::Transition;

void interactionWindowUsesTheRequestedTimeout() {
    assert(!display_brightness::isInteractionActive(100, 0));
    assert(display_brightness::isInteractionActive(100, 1));
    assert(display_brightness::isInteractionActive(10000, 1));
    assert(!display_brightness::isInteractionActive(10001, 1));
    assert(display_brightness::targetFor(false) == 3);
    assert(display_brightness::targetFor(true) == 15);
}

void interactionWindowSurvivesMillisRollover() {
    constexpr uint32_t lastInteraction = std::numeric_limits<uint32_t>::max() - 50;
    assert(display_brightness::isInteractionActive(25, lastInteraction));
    assert(!display_brightness::isInteractionActive(10050, lastInteraction));
}

void brightnessEasesOneStepAtATime() {
    Transition brightness;
    brightness.setTarget(3);
    assert(brightness.current() == 15);
    assert(!brightness.advance(74));
    assert(brightness.advance(75));
    assert(brightness.current() == 14);
    assert(!brightness.advance(149));
    assert(brightness.advance(150));
    assert(brightness.current() == 13);

    for (uint32_t now = 225; now <= 900; now += 75) {
        brightness.advance(now);
    }
    assert(brightness.current() == 3);
    assert(!brightness.advance(975));
}

void brightnessClampsAndCanRiseAgain() {
    Transition brightness{3};
    brightness.setTarget(99);
    assert(brightness.target() == 15);
    for (uint32_t now = 75; now <= 900; now += 75) {
        brightness.advance(now);
    }
    assert(brightness.current() == 15);
}

int main() {
    interactionWindowUsesTheRequestedTimeout();
    interactionWindowSurvivesMillisRollover();
    brightnessEasesOneStepAtATime();
    brightnessClampsAndCanRiseAgain();
    std::cout << "Display brightness tests passed\n";
}
