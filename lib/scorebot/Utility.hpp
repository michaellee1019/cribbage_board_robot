
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
