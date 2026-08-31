#pragma once

#include <stdint.h>

namespace scorebot {

inline constexpr char kPrinterEndpointUrl[] =
    "http://192.168.1.163:8099/text/print";
inline constexpr uint8_t kPrinterPayloadVersion = 1;
inline constexpr uint8_t kPrinterClientFormatVersion = 1;

}  // namespace scorebot
