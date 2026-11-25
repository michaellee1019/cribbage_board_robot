#pragma once

#include <cstdint>

namespace scorebot {

// Direct GPIO wiring belongs to this PCB identity independently of the selected color.
constexpr uint32_t kPlayerBoardV9Node = 0x76c3cfc0;
constexpr bool usesPlayerBoardV9(uint32_t node) { return node == kPlayerBoardV9Node; }
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
