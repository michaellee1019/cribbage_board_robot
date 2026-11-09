#pragma once

#include <cstdint>

namespace scorebot {

// ESP.getEfuseMac() stores the six MAC bytes in little-endian order. The
// original painlessMesh IDs in BoardRole.cpp are the final four displayed MAC
// bytes (for example, 98:3d:ae:ea:17:bc -> 0xaeea17bc).
constexpr uint32_t boardIdFromEfuseMac(uint64_t efuseMac) {
    return ((static_cast<uint32_t>(efuseMac >> 16) & 0x000000ffu) << 24) |
           ((static_cast<uint32_t>(efuseMac >> 16) & 0x0000ff00u) << 8) |
           ((static_cast<uint32_t>(efuseMac >> 16) & 0x00ff0000u) >> 8) |
           ((static_cast<uint32_t>(efuseMac >> 16) & 0xff000000u) >> 24);
}

// ESP32-S3 Arduino publishes the base MAC plus one or two, depending on how
// many universal addresses the board reserves. The low 32 bits of that base
// are the historical board id.
constexpr uint32_t boardIdFromBluetoothMac(uint64_t bluetoothMac, uint8_t offset) {
    return static_cast<uint32_t>((bluetoothMac - offset) & 0xffffffffu);
}

#if defined(CONFIG_ESP32S3_UNIVERSAL_MAC_ADDRESSES) && \
    CONFIG_ESP32S3_UNIVERSAL_MAC_ADDRESSES == 2
constexpr uint8_t kBluetoothMacOffset = 1;
#elif defined(CONFIG_ESP32S3_UNIVERSAL_MAC_ADDRESSES)
constexpr uint8_t kBluetoothMacOffset = 2;
#else
constexpr uint8_t kBluetoothMacOffset = 1;
#endif

}  // namespace scorebot
