#ifndef TYPES_HPP
#define TYPES_HPP

#include <Arduino.h>

using PlayerNumberT = int;
using ScoreT = long;
using TurnNumberT = int;
using TimestampT = decltype(millis());

template <typename T>
void print(const T& t) {
    Serial.print(t);
}

template <typename T>
void println(const T& t) {
    Serial.println(t);
}

inline void println() {
    Serial.println();
}

// TODO: Make this configurable.
static constexpr int MAX_DISPLAYS = 3;
static constexpr int MAX_PLAYERS = MAX_DISPLAYS;


#endif