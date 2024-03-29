#ifndef VIEW_HPP
#define VIEW_HPP

#include <TM1637Display.h>
#include <Arduino.h>

namespace scorebot::view {
class SegmentDisplay {
    enum class DisplayMode {
        kDecimal,
        kHex,
        kUnchanged,
        kClear,
    };
    using HexOrDecimal = union {
        int decimalValue;
        int hexValue;
    };

    DisplayMode mode{DisplayMode::kUnchanged};
    HexOrDecimal value{};
    TM1637Display display;

    bool changedBrightness{false};
    uint8_t brightness{0xFF};

public:
    explicit SegmentDisplay(TM1637Display&& display) : display{display} {}

    void setValueHex(const uint8_t hexValue) {
        this->mode = DisplayMode::kHex;
        this->value.hexValue = hexValue;
    }
    void setValueDec(const int decimalValue) {
        this->mode = DisplayMode::kDecimal;
        this->value.decimalValue = decimalValue;
    }
    void setBrightness(const uint8_t bval) {
        display.setBrightness(bval);
    }
    void clear() {
        display.clear();
    }
    void update() {
        if (mode == DisplayMode::kDecimal) {
            display.showNumberDec(value.decimalValue);
            this->mode = DisplayMode::kUnchanged;
        } else if (mode == DisplayMode::kHex) {
            display.showNumberHexEx(value.hexValue);
            this->mode = DisplayMode::kUnchanged;
        } else if (mode == DisplayMode::kClear) {
            display.clear();
            this->mode = DisplayMode::kUnchanged;
        }
    }
};

}  // namespace scorebot::view


#endif