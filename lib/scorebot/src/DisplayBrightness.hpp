#pragma once

#include <cstdint>

namespace display_brightness {

inline constexpr uint8_t kIdleBrightness = 0;
inline constexpr uint8_t kActiveBrightness = 15;
inline constexpr uint32_t kInteractionTimeoutMs = 5000;
inline constexpr uint32_t kBrightnessStepMs = 65;

inline bool isInteractionActive(uint32_t now, uint32_t lastInteractionMs) {
    return lastInteractionMs != 0 && now - lastInteractionMs < kInteractionTimeoutMs;
}

inline uint8_t targetFor(bool active) {
    return active ? kActiveBrightness : kIdleBrightness;
}

class Transition final {
public:
    explicit Transition(uint8_t initialBrightness = kActiveBrightness)
        : brightness(initialBrightness > kActiveBrightness ? kActiveBrightness : initialBrightness),
          targetBrightness(brightness) {}

    void setTarget(uint8_t newBrightness) {
        targetBrightness = newBrightness > kActiveBrightness ? kActiveBrightness : newBrightness;
    }

    bool advance(uint32_t now) {
        if (brightness == targetBrightness || now - lastStepMs < kBrightnessStepMs) {
            return false;
        }

        brightness += brightness < targetBrightness ? 1 : -1;
        lastStepMs = now;
        return true;
    }

    uint8_t current() const { return brightness; }
    uint8_t target() const { return targetBrightness; }

    void setCurrent(uint8_t newBrightness) {
        brightness = newBrightness > kActiveBrightness ? kActiveBrightness : newBrightness;
        targetBrightness = brightness;
    }

private:
    uint8_t brightness;
    uint8_t targetBrightness;
    uint32_t lastStepMs{0};
};

}  // namespace display_brightness
