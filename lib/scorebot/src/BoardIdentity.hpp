#pragma once

#include <cstdint>

namespace scorebot {

// ESP.getEfuseMac() stores the six MAC bytes in little-endian order. The
// original painlessMesh IDs in BoardRole.cpp are the final four displayed MAC
// bytes (for example, 98:3d:ae:ea:17:bc -> 0xaeea17bc).
constexpr uint32_t boardIdFromEfuseMac(uint64_t efuseMac) {
    const uint32_t tail = static_cast<uint32_t>(efuseMac >> 16);
    return ((tail & 0x000000ffu) << 24) |
           ((tail & 0x0000ff00u) << 8) |
           ((tail & 0x00ff0000u) >> 8) |
           ((tail & 0xff000000u) >> 24);
}

}  // namespace scorebot
