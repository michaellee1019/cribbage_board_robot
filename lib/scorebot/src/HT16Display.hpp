#ifndef HT16DISPLAY_H
#define HT16DISPLAY_H

#include <SparkFun_Alphanumeric_Display.h>
#include <cstdint>

class HT16Display {
    HT16K33 driver;

public:
    explicit HT16Display();
    void setup(uint8_t address);

    // Talks like a duck!
    template <typename... Args>
    auto print(Args&&... args) {
        return driver.print(std::forward<Args>(args)...);
    }
    void clear();
};


#endif
