#pragma once

#include <GameRules.hpp>
#include <PrinterConfig.hpp>

#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace scorebot {

inline constexpr char kPrinterTitle[] = "SCOREBOT";
inline constexpr char kPrinterFooter[] = "Printed {{Timestamp}}";

// Capacities include the trailing NUL. They are deliberately bounded so the
// production worker can serialize into stack-owned storage without allocation.
inline constexpr size_t kMaxPrinterFilenameSize = 64;
inline constexpr size_t kMaxPrinterLineSize = 24;
inline constexpr size_t kMaxPrinterRequestBodySize = 512;
inline constexpr size_t kPrinterSha256HexLength = 64;
inline constexpr size_t kMaxPrinterIdempotencyKeySize = 121;
inline constexpr size_t kMaxPrinterMessageSize = 14;

inline constexpr uint32_t kPrinterScrollStepMs = 300;
inline constexpr uint32_t kPrinterDoneDurationMs = 2000;
inline constexpr uint32_t kPrinterErrorDurationMs = 5000;
inline constexpr uint32_t kPrinterWifiTimeoutMs = 15000;
inline constexpr uint32_t kPrinterTcpConnectTimeoutMs = 5000;
inline constexpr uint32_t kPrinterHttpTimeoutMs = 15000;

struct PrinterSnapshot {
    uint32_t leaderId{0};
    uint32_t gameId{0};
    uint32_t term{0};
    uint32_t version{0};
    bool started{false};
    std::array<int32_t, kPlayerCount> scores{};
    Player turn{Player::None};
};

static_assert(std::is_trivially_copyable<PrinterSnapshot>::value,
              "printer snapshots must be safe to copy through a queue");
static_assert(std::is_standard_layout<PrinterSnapshot>::value,
              "printer snapshots must remain plain data");

enum class PrinterProgress : uint8_t {
    Idle,
    Wifi,
    Send,
    Done,
    Error,
};

enum class PrinterError : uint8_t {
    None,
    WifiFail,
    NoRoute,
    PrintTimeout,
    Http409,
    Http4xx,
    Http5xx,
    BadResponse,
    PrintBusy,
};

constexpr const char* printerPlayerName(Player player) {
    switch (player) {
        case Player::Red:
            return "RED";
        case Player::Blue:
            return "BLUE";
        case Player::Green:
            return "GREEN";
        case Player::White:
            return "WHITE";
        case Player::None:
            return "NONE";
    }
    return "NONE";
}

inline bool printerFormatSucceeded(int written, size_t capacity) {
    return written >= 0 && static_cast<size_t>(written) < capacity;
}

inline bool buildPrinterFilename(
    const PrinterSnapshot& snapshot, char* out, size_t capacity,
    size_t* length = nullptr) {
    if (out == nullptr || capacity == 0) {
        return false;
    }
    out[0] = '\0';
    const int written = std::snprintf(
        out, capacity, "scorebot-g%" PRIu32 "-t%" PRIu32 "-v%" PRIu32 ".png",
        snapshot.gameId, snapshot.term, snapshot.version);
    if (!printerFormatSucceeded(written, capacity)) {
        out[0] = '\0';
        return false;
    }
    if (length != nullptr) {
        *length = static_cast<size_t>(written);
    }
    return true;
}

inline bool buildPrinterScoreLine(
    Player player, int32_t score, char* out, size_t capacity,
    size_t* length = nullptr) {
    if (!isPlayer(player) || out == nullptr || capacity == 0) {
        if (out != nullptr && capacity != 0) {
            out[0] = '\0';
        }
        return false;
    }
    out[0] = '\0';
    const int written = std::snprintf(
        out, capacity, "%-5s%5" PRId32, printerPlayerName(player), score);
    if (!printerFormatSucceeded(written, capacity)) {
        out[0] = '\0';
        return false;
    }
    if (length != nullptr) {
        *length = static_cast<size_t>(written);
    }
    return true;
}

inline bool buildPrinterTurnLine(
    const PrinterSnapshot& snapshot, char* out, size_t capacity,
    size_t* length = nullptr) {
    if (out == nullptr || capacity == 0) {
        return false;
    }
    out[0] = '\0';
    const Player turn = snapshot.started && isPlayer(snapshot.turn)
                            ? snapshot.turn
                            : Player::None;
    const int written = std::snprintf(
        out, capacity, "TURN: %s", printerPlayerName(turn));
    if (!printerFormatSucceeded(written, capacity)) {
        out[0] = '\0';
        return false;
    }
    if (length != nullptr) {
        *length = static_cast<size_t>(written);
    }
    return true;
}

// The compact representation is intentional: its bytes are the stable input
// to SHA-256 and therefore part of the version-1 idempotency contract.
inline bool buildPrinterRequest(
    const PrinterSnapshot& snapshot, char* out, size_t capacity,
    size_t* length = nullptr) {
    if (out == nullptr || capacity == 0) {
        return false;
    }
    out[0] = '\0';

    std::array<char, kMaxPrinterFilenameSize> filename{};
    std::array<std::array<char, kMaxPrinterLineSize>, kPlayerCount> scoreLines{};
    std::array<char, kMaxPrinterLineSize> turnLine{};
    if (!buildPrinterFilename(snapshot, filename.data(), filename.size()) ||
        !buildPrinterTurnLine(snapshot, turnLine.data(), turnLine.size())) {
        return false;
    }
    for (size_t index = 0; index < kPlayerCount; ++index) {
        if (!buildPrinterScoreLine(
                static_cast<Player>(index), snapshot.scores[index],
                scoreLines[index].data(), scoreLines[index].size())) {
            return false;
        }
    }

    const int written = std::snprintf(
        out, capacity,
        "{\"version\":%u,\"filename\":\"%s\",\"title\":\"%s\","
        "\"lines\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"],"
        "\"footer\":\"%s\"}",
        static_cast<unsigned>(kPrinterPayloadVersion), filename.data(),
        kPrinterTitle, scoreLines[0].data(), scoreLines[1].data(),
        scoreLines[2].data(), scoreLines[3].data(), turnLine.data(),
        kPrinterFooter);
    if (!printerFormatSucceeded(written, capacity)) {
        out[0] = '\0';
        return false;
    }
    if (length != nullptr) {
        *length = static_cast<size_t>(written);
    }
    return true;
}

constexpr bool isLowercaseHex(char value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f');
}

inline bool isPrinterSha256Hex(const char* digest) {
    if (digest == nullptr ||
        std::strlen(digest) != kPrinterSha256HexLength) {
        return false;
    }
    for (size_t index = 0; index < kPrinterSha256HexLength; ++index) {
        if (!isLowercaseHex(digest[index])) {
            return false;
        }
    }
    return digest[kPrinterSha256HexLength] == '\0';
}

inline bool buildPrinterIdempotencyKey(
    const PrinterSnapshot& snapshot, const char* bodySha256Hex,
    char* out, size_t capacity, size_t* length = nullptr) {
    if (out == nullptr || capacity == 0) {
        return false;
    }
    out[0] = '\0';
    if (!isPrinterSha256Hex(bodySha256Hex)) {
        return false;
    }
    const int written = std::snprintf(
        out, capacity,
        "scorebot:p%u:%" PRIu32 ":%" PRIu32 ":%" PRIu32 ":%" PRIu32 ":%s",
        static_cast<unsigned>(kPrinterClientFormatVersion), snapshot.leaderId,
        snapshot.term, snapshot.gameId, snapshot.version, bodySha256Hex);
    if (!printerFormatSucceeded(written, capacity)) {
        out[0] = '\0';
        return false;
    }
    if (length != nullptr) {
        *length = static_cast<size_t>(written);
    }
    return true;
}

constexpr const char* printerProgressMessage(PrinterProgress progress) {
    switch (progress) {
        case PrinterProgress::Wifi:
            return "WIFI";
        case PrinterProgress::Send:
            return "SEND";
        case PrinterProgress::Done:
            return "DONE";
        case PrinterProgress::Idle:
        case PrinterProgress::Error:
            return "";
    }
    return "";
}

constexpr const char* printerErrorMessage(PrinterError error) {
    switch (error) {
        case PrinterError::WifiFail:
            return "WIFI FAIL";
        case PrinterError::NoRoute:
            return "NO ROUTE";
        case PrinterError::PrintTimeout:
            return "PRINT TIMEOUT";
        case PrinterError::Http409:
            return "HTTP 409";
        case PrinterError::Http4xx:
            return "HTTP 4XX";
        case PrinterError::Http5xx:
            return "HTTP 5XX";
        case PrinterError::BadResponse:
            return "BAD RESPONSE";
        case PrinterError::PrintBusy:
            return "PRINT BUSY";
        case PrinterError::None:
            return "";
    }
    return "";
}

constexpr PrinterError printerErrorForHttpResponse(
    int statusCode, bool validSuccessfulResponse) {
    if (statusCode >= 200 && statusCode <= 299) {
        return validSuccessfulResponse ? PrinterError::None
                                       : PrinterError::BadResponse;
    }
    if (statusCode == 409) {
        return PrinterError::Http409;
    }
    if (statusCode >= 400 && statusCode <= 499) {
        return PrinterError::Http4xx;
    }
    if (statusCode >= 500 && statusCode <= 599) {
        return PrinterError::Http5xx;
    }
    return PrinterError::BadResponse;
}

constexpr size_t printerMessageLength(const char* message) {
    if (message == nullptr) {
        return 0;
    }
    size_t length = 0;
    while (message[length] != '\0') {
        ++length;
    }
    return length;
}

// Long messages use the same cyclic convention as RESET: the text followed by
// three spaces. Thus each character fully exits a four-character display
// before the message repeats.
constexpr std::array<char, 5> printerMessageFrame(
    const char* message, uint32_t elapsedMs) {
    std::array<char, 5> frame = {' ', ' ', ' ', ' ', '\0'};
    const size_t length = printerMessageLength(message);
    if (length == 0) {
        return frame;
    }
    if (length <= 4) {
        for (size_t index = 0; index < length; ++index) {
            frame[index] = message[index];
        }
        return frame;
    }

    const size_t cycleLength = length + 3;
    const size_t offset = static_cast<size_t>(
        (elapsedMs / kPrinterScrollStepMs) % cycleLength);
    for (size_t index = 0; index < 4; ++index) {
        const size_t source = (offset + index) % cycleLength;
        frame[index] = source < length ? message[source] : ' ';
    }
    return frame;
}

constexpr std::array<char, 5> printerProgressFrame(
    PrinterProgress progress, uint32_t elapsedMs = 0) {
    return printerMessageFrame(printerProgressMessage(progress), elapsedMs);
}

constexpr std::array<char, 5> printerErrorFrame(
    PrinterError error, uint32_t elapsedMs) {
    return printerMessageFrame(printerErrorMessage(error), elapsedMs);
}

constexpr bool printerProgressIsTerminal(PrinterProgress progress) {
    return progress == PrinterProgress::Done ||
           progress == PrinterProgress::Error;
}

constexpr uint32_t printerTerminalDurationMs(PrinterProgress progress) {
    return progress == PrinterProgress::Done
               ? kPrinterDoneDurationMs
               : progress == PrinterProgress::Error
                     ? kPrinterErrorDurationMs
                     : 0;
}

constexpr uint32_t printerElapsedMs(uint32_t nowMs, uint32_t startedMs) {
    return static_cast<uint32_t>(nowMs - startedMs);
}

constexpr bool printerTerminalExpired(
    PrinterProgress progress, uint32_t startedMs, uint32_t nowMs) {
    return printerProgressIsTerminal(progress) &&
           printerElapsedMs(nowMs, startedMs) >=
               printerTerminalDurationMs(progress);
}

constexpr bool printerTerminalDismissedByInput(
    PrinterProgress progress, bool ordinaryLocalInput) {
    return ordinaryLocalInput && printerProgressIsTerminal(progress);
}

constexpr bool printerTransitionAllowed(
    PrinterProgress from, PrinterProgress to) {
    switch (from) {
        case PrinterProgress::Idle:
            return to == PrinterProgress::Wifi ||
                   to == PrinterProgress::Error;
        case PrinterProgress::Wifi:
            return to == PrinterProgress::Send ||
                   to == PrinterProgress::Error;
        case PrinterProgress::Send:
            return to == PrinterProgress::Done ||
                   to == PrinterProgress::Error;
        case PrinterProgress::Done:
        case PrinterProgress::Error:
            return to == PrinterProgress::Idle;
    }
    return false;
}

// Reset/restart, OTA, and an active maintenance hold all outrank the printer
// overlay. Normal game UI is lower priority than any non-idle printer phase.
constexpr bool shouldRenderPrinterOverlay(
    PrinterProgress progress, bool restartOrResetActive,
    bool otaActive, bool maintenanceHoldActive) {
    return progress != PrinterProgress::Idle && !restartOrResetActive &&
           !otaActive && !maintenanceHoldActive;
}

}  // namespace scorebot
