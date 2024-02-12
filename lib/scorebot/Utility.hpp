
#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <Arduino.h>

struct Button {
    int pin;
    explicit Button(int pin) : pin{pin} {}
    byte previous = HIGH;
    void setup() const {
        pinMode(pin, INPUT_PULLUP);
    }
    template <typename F>
    void update(F&& onPress) {
        byte state = digitalRead(pin);
        if (state == LOW && state != previous) {
            onPress();
        }
        previous = state;
    }
};

struct Periodically {
    const int period;
    explicit Periodically(int period) : period{period} {}
    unsigned long lastSent{0};
    template <typename F>
    void run(unsigned long now, F&& callback) {
        if (now >= lastSent + period) {
            callback();
            lastSent = now;
        }
    }
};

#endif //UTILITY_HPP
