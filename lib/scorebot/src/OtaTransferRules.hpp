#pragma once

#include <cstddef>
#include <cstdint>

namespace scorebot {

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
