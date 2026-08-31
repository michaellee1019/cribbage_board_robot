#include <PrinterRules.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>

using scorebot::Player;
using scorebot::PrinterError;
using scorebot::PrinterProgress;
using scorebot::PrinterSnapshot;

namespace {

PrinterSnapshot exampleSnapshot() {
    PrinterSnapshot snapshot{};
    snapshot.leaderId = 42;
    snapshot.gameId = 7;
    snapshot.term = 11;
    snapshot.version = 29;
    snapshot.started = true;
    snapshot.scores = {121, 96, 80, 67};
    snapshot.turn = Player::Red;
    return snapshot;
}

std::string filename(const PrinterSnapshot& snapshot) {
    std::array<char, scorebot::kMaxPrinterFilenameSize> out{};
    size_t length = 0;
    assert(scorebot::buildPrinterFilename(
        snapshot, out.data(), out.size(), &length));
    assert(length == std::strlen(out.data()));
    return out.data();
}

std::string scoreLine(Player player, int32_t score) {
    std::array<char, scorebot::kMaxPrinterLineSize> out{};
    assert(scorebot::buildPrinterScoreLine(
        player, score, out.data(), out.size()));
    return out.data();
}

std::string turnLine(const PrinterSnapshot& snapshot) {
    std::array<char, scorebot::kMaxPrinterLineSize> out{};
    assert(scorebot::buildPrinterTurnLine(
        snapshot, out.data(), out.size()));
    return out.data();
}

std::string request(const PrinterSnapshot& snapshot) {
    std::array<char, scorebot::kMaxPrinterRequestBodySize> out{};
    size_t length = 0;
    assert(scorebot::buildPrinterRequest(
        snapshot, out.data(), out.size(), &length));
    assert(length == std::strlen(out.data()));
    return out.data();
}

std::string key(const PrinterSnapshot& snapshot, const char* digest) {
    std::array<char, scorebot::kMaxPrinterIdempotencyKeySize> out{};
    size_t length = 0;
    assert(scorebot::buildPrinterIdempotencyKey(
        snapshot, digest, out.data(), out.size(), &length));
    assert(length == std::strlen(out.data()));
    return out.data();
}

std::string frame(const std::array<char, 5>& value) {
    return value.data();
}

void testSnapshotAndPayloadFields() {
    static_assert(std::is_trivially_copyable<PrinterSnapshot>::value, "POD");
    static_assert(std::is_standard_layout<PrinterSnapshot>::value, "POD");

    PrinterSnapshot snapshot = exampleSnapshot();
    assert(filename(snapshot) == "scorebot-g7-t11-v29.png");
    snapshot.gameId = std::numeric_limits<uint32_t>::max();
    snapshot.term = std::numeric_limits<uint32_t>::max();
    snapshot.version = std::numeric_limits<uint32_t>::max();
    assert(filename(snapshot) ==
           "scorebot-g4294967295-t4294967295-v4294967295.png");

    assert(scoreLine(Player::Red, 121) == "RED    121");
    assert(scoreLine(Player::Blue, 96) == "BLUE    96");
    assert(scoreLine(Player::Green, -7) == "GREEN   -7");
    assert(scoreLine(Player::White, std::numeric_limits<int32_t>::max()) ==
           "WHITE2147483647");
    assert(scoreLine(Player::White, std::numeric_limits<int32_t>::min()) ==
           "WHITE-2147483648");

    snapshot = exampleSnapshot();
    assert(turnLine(snapshot) == "TURN: RED");
    snapshot.turn = Player::Green;
    assert(turnLine(snapshot) == "TURN: GREEN");
    snapshot.started = false;
    assert(turnLine(snapshot) == "TURN: NONE");
    snapshot.started = true;
    snapshot.turn = static_cast<Player>(255);
    assert(turnLine(snapshot) == "TURN: NONE");

    char small[4] = {'x', 'x', 'x', '\0'};
    assert(!scorebot::buildPrinterFilename(snapshot, small, sizeof(small)));
    assert(small[0] == '\0');
    assert(!scorebot::buildPrinterScoreLine(
        Player::None, 0, small, sizeof(small)));
    assert(small[0] == '\0');
    assert(!scorebot::buildPrinterTurnLine(snapshot, nullptr, 0));
}

void testExactDeterministicRequest() {
    const PrinterSnapshot snapshot = exampleSnapshot();
    const std::string expected =
        "{\"version\":1,\"filename\":\"scorebot-g7-t11-v29.png\","
        "\"title\":\"SCOREBOT\",\"lines\":[\"RED    121\","
        "\"BLUE    96\",\"GREEN   80\",\"WHITE   67\",\"TURN: RED\"],"
        "\"footer\":\"Printed {{Timestamp}}\"}";
    assert(request(snapshot) == expected);
    assert(request(snapshot) == request(snapshot));

    PrinterSnapshot changed = snapshot;
    changed.gameId++;
    assert(request(changed) != expected);
    changed = snapshot;
    changed.term++;
    assert(request(changed) != expected);
    changed = snapshot;
    changed.version++;
    assert(request(changed) != expected);
    changed = snapshot;
    changed.scores[2]--;
    assert(request(changed) != expected);
    changed = snapshot;
    changed.turn = Player::White;
    assert(request(changed) != expected);
    changed = snapshot;
    changed.started = false;
    assert(request(changed).find("TURN: NONE") != std::string::npos);

    std::array<char, 8> tooSmall{};
    assert(!scorebot::buildPrinterRequest(
        snapshot, tooSmall.data(), tooSmall.size()));
    assert(tooSmall[0] == '\0');
    assert(!scorebot::buildPrinterRequest(snapshot, nullptr, 0));
}

void testIdempotencyKey() {
    constexpr char digest[] =
        "0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef";
    const PrinterSnapshot snapshot = exampleSnapshot();
    const std::string expected =
        "scorebot:p1:42:11:7:29:"
        "0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef";
    assert(key(snapshot, digest) == expected);
    assert(key(snapshot, digest) == key(snapshot, digest));

    PrinterSnapshot changed = snapshot;
    changed.leaderId++;
    assert(key(changed, digest) != expected);
    changed = snapshot;
    changed.term++;
    assert(key(changed, digest) != expected);
    changed = snapshot;
    changed.gameId++;
    assert(key(changed, digest) != expected);
    changed = snapshot;
    changed.version++;
    assert(key(changed, digest) != expected);

    constexpr char otherDigest[] =
        "1123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef";
    assert(key(snapshot, otherDigest) != expected);

    assert(scorebot::isPrinterSha256Hex(digest));
    assert(!scorebot::isPrinterSha256Hex(nullptr));
    assert(!scorebot::isPrinterSha256Hex("abc"));
    constexpr char uppercaseDigest[] =
        "0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdeF";
    assert(!scorebot::isPrinterSha256Hex(uppercaseDigest));

    std::array<char, scorebot::kMaxPrinterIdempotencyKeySize> out{};
    assert(!scorebot::buildPrinterIdempotencyKey(
        snapshot, "bad", out.data(), out.size()));
    assert(out[0] == '\0');
    assert(!scorebot::buildPrinterIdempotencyKey(
        snapshot, digest, out.data(), expected.size()));
    assert(out[0] == '\0');

    PrinterSnapshot maximum{};
    maximum.leaderId = std::numeric_limits<uint32_t>::max();
    maximum.term = std::numeric_limits<uint32_t>::max();
    maximum.gameId = std::numeric_limits<uint32_t>::max();
    maximum.version = std::numeric_limits<uint32_t>::max();
    assert(key(maximum, digest).size() + 1 ==
           scorebot::kMaxPrinterIdempotencyKeySize);
}

void testMessagesAndHttpClassification() {
    assert(std::string(scorebot::printerProgressMessage(PrinterProgress::Idle)).empty());
    assert(std::string(scorebot::printerProgressMessage(PrinterProgress::Wifi)) == "WIFI");
    assert(std::string(scorebot::printerProgressMessage(PrinterProgress::Send)) == "SEND");
    assert(std::string(scorebot::printerProgressMessage(PrinterProgress::Done)) == "DONE");
    assert(std::string(scorebot::printerProgressMessage(PrinterProgress::Error)).empty());

    const std::array<std::pair<PrinterError, const char*>, 9> messages = {{
        {PrinterError::None, ""},
        {PrinterError::WifiFail, "WIFI FAIL"},
        {PrinterError::NoRoute, "NO ROUTE"},
        {PrinterError::PrintTimeout, "PRINT TIMEOUT"},
        {PrinterError::Http409, "HTTP 409"},
        {PrinterError::Http4xx, "HTTP 4XX"},
        {PrinterError::Http5xx, "HTTP 5XX"},
        {PrinterError::BadResponse, "BAD RESPONSE"},
        {PrinterError::PrintBusy, "PRINT BUSY"},
    }};
    for (const auto& entry : messages) {
        assert(std::string(scorebot::printerErrorMessage(entry.first)) ==
               entry.second);
        assert(std::strlen(entry.second) + 1 <= scorebot::kMaxPrinterMessageSize);
    }

    assert(scorebot::printerErrorForHttpResponse(200, true) == PrinterError::None);
    assert(scorebot::printerErrorForHttpResponse(299, true) == PrinterError::None);
    assert(scorebot::printerErrorForHttpResponse(204, false) ==
           PrinterError::BadResponse);
    assert(scorebot::printerErrorForHttpResponse(409, false) ==
           PrinterError::Http409);
    assert(scorebot::printerErrorForHttpResponse(400, false) ==
           PrinterError::Http4xx);
    assert(scorebot::printerErrorForHttpResponse(499, false) ==
           PrinterError::Http4xx);
    assert(scorebot::printerErrorForHttpResponse(500, false) ==
           PrinterError::Http5xx);
    assert(scorebot::printerErrorForHttpResponse(599, false) ==
           PrinterError::Http5xx);
    assert(scorebot::printerErrorForHttpResponse(302, false) ==
           PrinterError::BadResponse);
    assert(scorebot::printerErrorForHttpResponse(-1, false) ==
           PrinterError::BadResponse);
}

void testScrollingFrames() {
    assert(frame(scorebot::printerProgressFrame(PrinterProgress::Wifi)) == "WIFI");
    assert(frame(scorebot::printerProgressFrame(PrinterProgress::Send)) == "SEND");
    assert(frame(scorebot::printerProgressFrame(PrinterProgress::Done)) == "DONE");
    assert(frame(scorebot::printerProgressFrame(PrinterProgress::Idle)) == "    ");

    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 0)) == "WIFI");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 299)) == "WIFI");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 300)) == "IFI ");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 600)) == "FI F");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 2100)) == "IL  ");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 2400)) == "L   ");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 2700)) == "   W");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 3000)) == "  WI");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 3300)) == " WIF");
    assert(frame(scorebot::printerErrorFrame(PrinterError::WifiFail, 3600)) == "WIFI");

    // Exercise every frame of every fixed error cycle, including its wrap.
    const std::array<PrinterError, 8> errors = {
        PrinterError::WifiFail, PrinterError::NoRoute,
        PrinterError::PrintTimeout, PrinterError::Http409,
        PrinterError::Http4xx, PrinterError::Http5xx,
        PrinterError::BadResponse, PrinterError::PrintBusy,
    };
    for (const PrinterError error : errors) {
        const std::string message = scorebot::printerErrorMessage(error);
        const size_t cycleLength = message.size() + 3;
        for (size_t offset = 0; offset < cycleLength; ++offset) {
            std::string expected;
            for (size_t column = 0; column < 4; ++column) {
                const size_t source = (offset + column) % cycleLength;
                expected.push_back(source < message.size() ? message[source] : ' ');
            }
            assert(frame(scorebot::printerErrorFrame(
                       error, static_cast<uint32_t>(
                                  offset * scorebot::kPrinterScrollStepMs))) ==
                   expected);
        }
        assert(frame(scorebot::printerErrorFrame(
                   error, static_cast<uint32_t>(
                              cycleLength * scorebot::kPrinterScrollStepMs))) ==
               message.substr(0, 4));
    }
}

void testWorkflowAndTiming() {
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Idle, PrinterProgress::Wifi));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Idle, PrinterProgress::Error));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Wifi, PrinterProgress::Send));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Wifi, PrinterProgress::Error));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Send, PrinterProgress::Done));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Send, PrinterProgress::Error));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Done, PrinterProgress::Idle));
    assert(scorebot::printerTransitionAllowed(
        PrinterProgress::Error, PrinterProgress::Idle));
    assert(!scorebot::printerTransitionAllowed(
        PrinterProgress::Idle, PrinterProgress::Done));
    assert(!scorebot::printerTransitionAllowed(
        PrinterProgress::Wifi, PrinterProgress::Done));
    assert(!scorebot::printerTransitionAllowed(
        PrinterProgress::Done, PrinterProgress::Wifi));
    assert(!scorebot::printerTransitionAllowed(
        PrinterProgress::Send, PrinterProgress::Send));

    assert(!scorebot::printerProgressIsTerminal(PrinterProgress::Wifi));
    assert(scorebot::printerProgressIsTerminal(PrinterProgress::Done));
    assert(scorebot::printerProgressIsTerminal(PrinterProgress::Error));
    assert(scorebot::printerTerminalDurationMs(PrinterProgress::Done) == 2000);
    assert(scorebot::printerTerminalDurationMs(PrinterProgress::Error) == 5000);
    assert(scorebot::printerTerminalDurationMs(PrinterProgress::Send) == 0);

    assert(!scorebot::printerTerminalExpired(PrinterProgress::Done, 100, 2099));
    assert(scorebot::printerTerminalExpired(PrinterProgress::Done, 100, 2100));
    assert(!scorebot::printerTerminalExpired(PrinterProgress::Error, 100, 5099));
    assert(scorebot::printerTerminalExpired(PrinterProgress::Error, 100, 5100));
    assert(!scorebot::printerTerminalExpired(PrinterProgress::Send, 100, 100000));

    // Unsigned subtraction makes elapsed-time decisions safe across millis().
    constexpr uint32_t nearWrap = std::numeric_limits<uint32_t>::max() - 999;
    assert(scorebot::printerElapsedMs(500, nearWrap) == 1500);
    assert(!scorebot::printerTerminalExpired(
        PrinterProgress::Done, nearWrap, 500));
    assert(scorebot::printerTerminalExpired(
        PrinterProgress::Done, nearWrap, 1000));

    assert(scorebot::printerTerminalDismissedByInput(
        PrinterProgress::Done, true));
    assert(scorebot::printerTerminalDismissedByInput(
        PrinterProgress::Error, true));
    assert(!scorebot::printerTerminalDismissedByInput(
        PrinterProgress::Send, true));
    assert(!scorebot::printerTerminalDismissedByInput(
        PrinterProgress::Done, false));

    assert(scorebot::shouldRenderPrinterOverlay(
        PrinterProgress::Wifi, false, false, false));
    assert(!scorebot::shouldRenderPrinterOverlay(
        PrinterProgress::Idle, false, false, false));
    assert(!scorebot::shouldRenderPrinterOverlay(
        PrinterProgress::Wifi, true, false, false));
    assert(!scorebot::shouldRenderPrinterOverlay(
        PrinterProgress::Wifi, false, true, false));
    assert(!scorebot::shouldRenderPrinterOverlay(
        PrinterProgress::Wifi, false, false, true));
}

}  // namespace

int main() {
    testSnapshotAndPayloadFields();
    testExactDeterministicRequest();
    testIdempotencyKey();
    testMessagesAndHttpClassification();
    testScrollingFrames();
    testWorkflowAndTiming();
    std::cout << "Printer-rule tests passed\n";
}
