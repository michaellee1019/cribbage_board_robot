#ifndef TYPES_HPP
#define TYPES_HPP

#include <Arduino.h>

using PlayerNumberT = int;
using ScoreT = int;
using TurnNumberT = int;
using TimestampT = decltype(millis());

template <typename T>
void print(const T& t) {
    Serial.print(t);
}

// TODO: Make this configurable.
static constexpr int N_DISPLAYS = 3;
static constexpr int N_PLAYERS = N_DISPLAYS;


#endif