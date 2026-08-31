#include <DisplayBrightness.hpp>

#include <cassert>
#include <iostream>
#include <limits>

using display_brightness::Transition;

void interactionWindowUsesTheRequestedTimeout() {
    assert(!display_brightness::isInteractionActive(100, 0));
    assert(display_brightness::isInteractionActive(100, 1));
    assert(display_brightness::isInteractionActive(5000, 1));
    assert(!display_brightness::isInteractionActive(5001, 1));
    assert(display_brightness::targetFor(false) == 0);
    assert(display_brightness::targetFor(true) == 15);
}

void interactionWindowSurvivesMillisRollover() {
    constexpr uint32_t lastInteraction = std::numeric_limits<uint32_t>::max() - 50;
    assert(display_brightness::isInteractionActive(25, lastInteraction));
    assert(!display_brightness::isInteractionActive(5050, lastInteraction));
}

void brightnessEasesOneStepAtATime() {
    Transition brightness;
    brightness.setTarget(0);
    assert(brightness.current() == 15);
    assert(!brightness.advance(64));
    assert(brightness.advance(65));
    assert(brightness.current() == 14);
    assert(!brightness.advance(129));
    assert(brightness.advance(130));
    assert(brightness.current() == 13);

    for (uint32_t now = 195; now <= 975; now += 65) {
        brightness.advance(now);
    }
    assert(brightness.current() == 0);
    assert(!brightness.advance(1040));
}

void brightnessClampsAndCanRiseAgain() {
    Transition brightness{0};
    brightness.setTarget(99);
    assert(brightness.target() == 15);
    for (uint32_t now = 65; now <= 975; now += 65) {
        brightness.advance(now);
    }
    assert(brightness.current() == 15);

    brightness.setCurrent(99);
    assert(brightness.current() == 15);
    assert(brightness.target() == 15);
}

int main() {
    interactionWindowUsesTheRequestedTimeout();
    interactionWindowSurvivesMillisRollover();
    brightnessEasesOneStepAtATime();
    brightnessClampsAndCanRiseAgain();
    std::cout << "Display brightness tests passed\n";
}
