
#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <Arduino.h>

struct Button {
    const int pin;
    explicit Button(int pin) : pin{pin} {}
    byte previous = HIGH;
    void setup() const {
        pinMode(pin, INPUT_PULLUP);
    }
    template <typename F>
    void onLoop(F&& onPress) {
        byte state = digitalRead(pin);
        if (state == LOW && state != previous) {
            onPress();
        }
        previous = state;
    }
};

struct Light {
    const int pin;
    explicit Light(int pin) : pin{pin} {}
    void setup() const {
        pinMode(pin, OUTPUT);
    }
    void turnOn() const {
        digitalWrite(pin, HIGH);
    }
    void turnOff() const {
        digitalWrite(pin, LOW);
    }
};

template <size_t n>
struct DipSwitches {
    int pins[n];

    template <typename... Args>
    explicit DipSwitches(Args... args) : pins{args...} {
        static_assert(sizeof...(args) == n, "Incorrect number of arguments for DipSwitches");
    }
    void setup() const {
        for (const auto i : pins) {
            pinMode(i, INPUT_PULLUP);
        }
    }

    int value() {
        int out = 0;
        for (size_t i = 0; i < n; ++i) {
            out = out | (digitalRead(pins[i]) == LOW ? 0 : 1) << i;
        }
        return out;
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

#endif  // UTILITY_HPP
