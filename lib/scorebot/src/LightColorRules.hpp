#pragma once

#include <cstdint>

namespace scorebot {

constexpr uint8_t kActivePlayerLightLevel = 32;

constexpr uint32_t scaleRgb(uint32_t color, uint8_t level) {
    const uint32_t red = ((color >> 16) & 0xffu) * level / 255u;
    const uint32_t green = ((color >> 8) & 0xffu) * level / 255u;
    const uint32_t blue = (color & 0xffu) * level / 255u;
    return (red << 16) | (green << 8) | blue;
}

constexpr uint32_t activePlayerColor(uint32_t color) {
    return scaleRgb(color, kActivePlayerLightLevel);
}

}  // namespace scorebot
