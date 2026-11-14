#pragma once

#include <BoardRole.hpp>
#include <cstdint>

namespace scorebot {

// The player PCB wires buttons, the encoder, the display, and the side LEDs
// directly to the ESP32-S3. The leaderboard keeps the earlier MCP23017 and
// seesaw board, so only non-leader roles take this path. A module whose MAC
// is not in the role table yet is still built on this PCB.
bool usesPlayerBoardHardware();

namespace player_pins {
constexpr int kSda = 17;
constexpr int kScl = 18;
constexpr int kBoostEnable = 47;
constexpr int kBatterySense = 1;
constexpr int kVbusSense = 2;
constexpr int kNeoPixel = 48;
constexpr int kNeoPixelCount = 8;
constexpr uint8_t kDisplayAddress = 0x70;

// GPIO order matches the logical button bits used by ButtonGrid:
// ADD, -1, +5, +1, OK.
constexpr int kButtonGpio[] = {4, 5, 6, 7, 8};
constexpr int kButtonCount = 5;

constexpr int kEncoderA = 9;
constexpr int kEncoderB = 10;
constexpr int kEncoderSwitch = 11;

constexpr uint64_t kWakeMask =
    (1ULL << 4) | (1ULL << 5) | (1ULL << 6) | (1ULL << 7) |
    (1ULL << 8) | (1ULL << 9) | (1ULL << 10) | (1ULL << 11);
}  // namespace player_pins

void enablePlayerBoost();
void disablePlayerBoost();

}  // namespace scorebot
