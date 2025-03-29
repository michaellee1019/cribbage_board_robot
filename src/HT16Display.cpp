#include <HT16Display.hpp>

HT16Display::HT16Display() = default;

void HT16Display::setup(uint8_t address) {
    while (!driver.begin(address)) {
    }
}