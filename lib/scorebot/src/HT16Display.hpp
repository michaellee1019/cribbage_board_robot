#ifndef HT16DISPLAY_H
#define HT16DISPLAY_H

#include <SparkFun_Alphanumeric_Display.h>
#include <DisplayBrightness.hpp>
#include <I2cBus.hpp>
#include <cstdint>
#include <utility>

class HT16Display {
    HT16K33 driver;
    display_brightness::Transition brightness;
    bool initialized{false};
    float blinkRateHz{-1.0f};

public:
    explicit HT16Display();
    void setup(uint8_t address);

    // Talks like a duck!
    template <typename... Args>
    auto print(Args&&... args) {
        I2cBus::Guard guard;
        return driver.print(std::forward<Args>(args)...);
    }
    void clear();
    void sleep();
    void setTargetBrightness(uint8_t brightness);
    void setBrightnessNow(uint8_t brightness);
    void setBlinkRate(float rateHz);
    void updateBrightness(uint32_t now);
    uint8_t currentBrightness() const;
};


#endif
