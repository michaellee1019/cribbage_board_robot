#include <HT16Display.hpp>
#include <ErrorHandler.hpp>

HT16Display::HT16Display() = default;

void HT16Display::setup(uint8_t address) {
    const int maxRetries = 10;
    const int delayMs = 500;
    
    for (int retry = 0; retry < maxRetries; retry++) {
        I2cBus::Guard guard;
        if (driver.begin(address)) {
            initialized = true;
            blinkRateHz = 0.0f;
            return;
        }
        delay(delayMs);
    }
    
    FATAL_ERROR(ErrorCode::DISPLAY_INIT_FAILED, "HT16Display initialization failed after retries");
}
void HT16Display::clear() {
    I2cBus::Guard guard;
    driver.clear();
}

void HT16Display::sleep() {
    if (!initialized) {
        return;
    }
    I2cBus::Guard guard;
    driver.clear();
    driver.displayOff();
    driver.disableSystemClock();
}

void HT16Display::setTargetBrightness(uint8_t newBrightness) {
    brightness.setTarget(newBrightness);
}

void HT16Display::setBrightnessNow(uint8_t newBrightness) {
    const uint8_t clamped = newBrightness > display_brightness::kActiveBrightness
                                ? display_brightness::kActiveBrightness
                                : newBrightness;
    brightness.setCurrent(clamped);
    if (!initialized) {
        return;
    }
    I2cBus::Guard guard;
    driver.setBrightness(clamped);
}

void HT16Display::setBlinkRate(float rateHz) {
    if (!initialized || blinkRateHz == rateHz) {
        return;
    }
    I2cBus::Guard guard;
    driver.setBlinkRate(rateHz);
    blinkRateHz = rateHz;
}

uint8_t HT16Display::currentBrightness() const {
    return brightness.current();
}

void HT16Display::updateBrightness(uint32_t now) {
    if (!initialized || !brightness.advance(now)) {
        return;
    }

    {
        I2cBus::Guard guard;
        driver.setBrightness(brightness.current());
    }
}
