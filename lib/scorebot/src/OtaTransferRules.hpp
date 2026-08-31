#pragma once

#include <cstddef>
#include <cstdint>

namespace scorebot {

inline constexpr uint32_t kOtaPurple = 0xff00ff;
inline constexpr uint32_t kOtaBlinkIntervalMs = 250;

constexpr uint32_t otaIndicatorColor(bool writing, uint32_t nowMs) {
    return !writing || ((nowMs / kOtaBlinkIntervalMs) & 1u) == 0
               ? kOtaPurple
               : 0;
}

constexpr bool otaArmHoldReached(bool pressed, uint32_t nowMs,
                                 uint32_t pressStartedMs, uint32_t holdMs) {
    return pressed && pressStartedMs != 0 &&
           static_cast<uint32_t>(nowMs - pressStartedMs) >= holdMs;
}

constexpr bool otaTransferOwnedBy(bool writing, uint16_t writer, uint16_t connection) {
    return writing && writer == connection;
}

constexpr bool otaCanAppend(bool writing, uint16_t writer, uint16_t connection,
                            uint32_t received, uint32_t expected, size_t length) {
    return otaTransferOwnedBy(writing, writer, connection) && length != 0 &&
           received <= expected && length <= expected - received;
}

constexpr bool otaTransferTimedOut(bool writing, uint32_t nowMs,
                                   uint32_t lastProgressMs, uint32_t timeoutMs) {
    return writing && static_cast<uint32_t>(nowMs - lastProgressMs) > timeoutMs;
}

}  // namespace scorebot
