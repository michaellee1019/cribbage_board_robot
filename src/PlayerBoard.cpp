#include <PlayerBoard.hpp>

#include <BoardIdentity.hpp>

#include <Arduino.h>

namespace scorebot {

bool usesPlayerBoardHardware() {
    const uint32_t id = boardIdFromEfuseMac(ESP.getEfuseMac());
    return getRoleConfig(id).role != BoardRole::Leader;
}

void enablePlayerBoost() {
    // Hold the LED data line low until the boost has risen, and leave the
    // resistor dividers as plain analog inputs so the pulls do not bias them.
    pinMode(player_pins::kNeoPixel, OUTPUT);
    digitalWrite(player_pins::kNeoPixel, LOW);
    pinMode(player_pins::kBatterySense, INPUT);
    pinMode(player_pins::kVbusSense, INPUT);
    pinMode(player_pins::kBoostEnable, OUTPUT);
    digitalWrite(player_pins::kBoostEnable, HIGH);
}

void disablePlayerBoost() {
    pinMode(player_pins::kNeoPixel, OUTPUT);
    digitalWrite(player_pins::kNeoPixel, LOW);
    pinMode(player_pins::kBoostEnable, OUTPUT);
    digitalWrite(player_pins::kBoostEnable, LOW);
}

}  // namespace scorebot
