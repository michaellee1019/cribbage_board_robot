#ifndef HT16DISPLAY_H
#define HT16DISPLAY_H

#include <SparkFun_Alphanumeric_Display.h>
#include <cstdint>

class HT16Display {
    HT16K33 driver;

public:
    HT16Display() = default;
    void setup(uint8_t address) {
        while (!driver.begin(address)) {
        }
    }

    // Talks like a duck!
    template <typename... Args>
    auto print(Args&&... args) {
        Serial.println("PRINT EVENT");
        return driver.print(std::forward<Args>(args)...);
    }
};


#endif
