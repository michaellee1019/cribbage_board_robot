#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace scorebot {
// Local, opt-in debug firmware only. One bounded raw MCP tap per line.
constexpr uint8_t kUsbState = 5;
constexpr uint8_t kUsbPair = 6;
class UsbTapParser {
    std::array<char, 8> line{};
    unsigned size{0};
    bool overflow{false};
public:
    uint8_t feed(char ch) {
        if (ch != '\n') {
            if (size < line.size()) line[size++] = ch;
            else overflow = true;
            return 0xff;
        }
        if (size && line[size - 1] == '\r') --size;
        const bool valid = !overflow && size == 5 && line[0] == 'T' && line[1] == 'A' &&
            line[2] == 'P' && line[3] == ' ' && line[4] >= '0' && line[4] <= '4';
        const std::string_view command(line.data(), size);
        const uint8_t pin = valid ? static_cast<uint8_t>(line[4] - '0') :
            !overflow && command == "STATE" ? kUsbState :
            !overflow && command == "PAIR" ? kUsbPair : 0xff;
        size = 0;
        overflow = false;
        return pin;
    }
};
}  // namespace scorebot
