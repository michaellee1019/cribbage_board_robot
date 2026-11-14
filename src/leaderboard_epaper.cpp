#include <Arduino.h>
#include <BoardIdentity.hpp>
#include <BoardRole.hpp>
#include <Adafruit_GFX.h>
#include <EpaperRefreshRules.hpp>
#include <LeaderboardBroadcast.hpp>
#include <NimBLEDevice.h>
#include <SPI.h>

#include "LeaderboardPanel.hpp"

namespace {
constexpr int kMosi = 11;
constexpr int kClock = 12;
constexpr int kChipSelect = 10;
constexpr int kDataCommand = 9;
constexpr int kReset = 14;
constexpr int kBusy = 13;
constexpr uint32_t kBroadcastTimeoutMs = 12000;
constexpr uint32_t kStartupListenMs = 2500;
constexpr uint32_t kPanelRetryMs = 30000;
constexpr uint8_t kBluetoothMacOffset =
    scorebot::bluetoothMacOffset(CONFIG_ESP32S3_UNIVERSAL_MAC_ADDRESSES);
constexpr std::array<const char*, scorebot::kPlayerCount> kPlayerNames{
    "RED", "BLUE", "GREEN", "WHITE"};

LeaderboardPanel panel{kChipSelect, kDataCommand, kReset, kBusy};
GFXcanvas1 display{LeaderboardPanel::WIDTH, LeaderboardPanel::HEIGHT};
GFXcanvas1 redHeader{LeaderboardPanel::WIDTH, LeaderboardPanel::HEIGHT};
std::array<uint8_t, LeaderboardPanel::WIDTH / 8 * LeaderboardPanel::HEIGHT> displayedPixels{};

struct Reception {
    scorebot::LeaderboardBroadcast leaderboard;
    uint32_t receivedAtMs;
};

QueueHandle_t latestReception;
NimBLEScan* scanner;
uint32_t scanStartedMs;

class LeaderboardScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        const NimBLEAddress& address = device->getAddress();
        if (!device->haveManufacturerData() || !address.isPublic() ||
            getRoleConfigForBluetoothMac(static_cast<uint64_t>(address), kBluetoothMacOffset).role !=
                BoardRole::Leader) {
            return;
        }
        const std::string data = device->getManufacturerData();
        Reception reception{};
        if (scorebot::decodeLeaderboard(reinterpret_cast<const uint8_t*>(data.data()),
                                        data.size(), reception.leaderboard)) {
            reception.receivedAtMs = millis();
            xQueueOverwrite(latestReception, &reception);
        }
    }
};

LeaderboardScanCallbacks scanCallbacks;

struct DisplayFrame {
    scorebot::LeaderboardBroadcast leaderboard{};
    bool received{false};
    bool live{false};

    bool operator==(const DisplayFrame& other) const {
        return received == other.received && live == other.live &&
               leaderboard.rosterMask == other.leaderboard.rosterMask &&
               leaderboard.turn == other.leaderboard.turn &&
               leaderboard.scores == other.leaderboard.scores;
    }
};

void drawHeader() {
    redHeader.fillScreen(1);
    redHeader.setTextColor(0);
    redHeader.setTextSize(2);
    redHeader.setCursor(12, 9);
    redHeader.print("Cribbage");
    // In landscape, the panel aligns partial windows to eight-pixel bands
    // along Y. Keep the red pixels above Y=32 and the dynamic content below it.
    redHeader.drawFastHLine(12, 31, 272, 0);
}

void draw(const DisplayFrame& frame) {
    display.fillScreen(1);
    display.setTextColor(0);
    display.setTextSize(1);
    display.setCursor(220, 36);
    display.print(frame.live ? "LIVE" : frame.received ? "LAST SEEN" : "LISTENING");

    const auto& leaderboard = frame.leaderboard;
    if (leaderboard.rosterMask == 0) {
        display.setTextSize(2);
        display.setCursor(12, 56);
        display.print(frame.received ? "Waiting for a game" : "Waiting for host");
    } else {
        int16_t y = 47;
        for (size_t i = 0; i < kPlayerNames.size(); ++i) {
            if ((leaderboard.rosterMask & (1u << i)) == 0) {
                continue;
            }
            const bool active = leaderboard.turn == static_cast<scorebot::Player>(i);
            if (active) {
                display.fillRect(8, y - 2, 280, 19, 0);
            }
            display.setTextColor(active ? 1 : 0);
            display.setTextSize(2);
            display.setCursor(12, y);
            display.print(kPlayerNames[i]);

            char score[12];
            snprintf(score, sizeof(score), "%ld", static_cast<long>(leaderboard.scores[i]));
            display.setCursor(284 - 12 * strlen(score), y);
            display.print(score);
            if (active) {
                display.setTextSize(1);
                display.setCursor(113, y + 4);
                display.print("TURN");
            }
            y += 20;
        }
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("Scorebot e-paper listener %08lx, flash %lu MiB, PSRAM heap %lu KiB\n",
                  static_cast<unsigned long>(scorebot::boardIdFromEfuseMac(ESP.getEfuseMac())),
                  static_cast<unsigned long>(ESP.getFlashChipSize() >> 20),
                  static_cast<unsigned long>(ESP.getPsramSize() >> 10));
    SPI.begin(kClock, -1, kMosi, kChipSelect);
    panel.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    panel.init(0, false, 5);
    display.setRotation(1);
    display.setTextWrap(false);
    redHeader.setRotation(1);
    drawHeader();

    latestReception = xQueueCreate(1, sizeof(Reception));
    if (latestReception == nullptr || !NimBLEDevice::init("Scorebot E-paper")) {
        Serial.println("Could not initialize BLE listener");
        return;
    }
    scanner = NimBLEDevice::getScan();
    scanner->setActiveScan(false);
    scanner->setInterval(100);
    scanner->setWindow(90);
    scanner->setMaxResults(0);
    scanner->setScanCallbacks(&scanCallbacks, true);
    scanStartedMs = millis();
    Serial.println("Listening for the Scorebot host broadcast");
}

void loop() {
    static DisplayFrame frame{};
    static DisplayFrame displayed{};
    static bool hasBaseline = false;
    static bool panelFailed = false;
    static unsigned partials = 0;
    static uint32_t lastFullMs;
    static uint32_t lastChangeMs;
    static uint32_t lastFailureMs;
    static uint32_t lastReceptionMs;
    if (scanner != nullptr && !scanner->isScanning()) {
        if (!scanner->start(0)) {
            delay(1000);
        }
    }
    const DisplayFrame previous = frame;
    Reception reception{};
    if (latestReception != nullptr && xQueueReceive(latestReception, &reception, 0) == pdPASS) {
        const auto& incoming = reception.leaderboard;
        if (!frame.live || incoming.rosterMask != frame.leaderboard.rosterMask ||
            incoming.turn != frame.leaderboard.turn || incoming.scores != frame.leaderboard.scores) {
            Serial.printf("Host broadcast: roster=0x%x turn=%u scores=[%ld,%ld,%ld,%ld]\n",
                          static_cast<unsigned>(incoming.rosterMask),
                          static_cast<unsigned>(incoming.turn),
                          static_cast<long>(incoming.scores[0]), static_cast<long>(incoming.scores[1]),
                          static_cast<long>(incoming.scores[2]), static_cast<long>(incoming.scores[3]));
        }
        frame.leaderboard = reception.leaderboard;
        frame.received = true;
        lastReceptionMs = reception.receivedAtMs;
    }
    const uint32_t now = millis();
    frame.live = frame.received && now - lastReceptionMs < kBroadcastTimeoutMs;
    if (!(frame == previous)) {
        lastChangeMs = now;
    }
    const bool ready = display.getBuffer() != nullptr && redHeader.getBuffer() != nullptr &&
                       (hasBaseline || frame.received || now - scanStartedMs >= kStartupListenMs) &&
                       (!panelFailed || now - lastFailureMs >= kPanelRetryMs);
    const auto refresh = ready
                             ? scorebot::epaperRefresh(hasBaseline, !(frame == displayed), partials,
                                                       now - lastChangeMs, now - lastFullMs)
                             : scorebot::EpaperRefresh::None;
    if (refresh != scorebot::EpaperRefresh::None) {
        const bool full = refresh == scorebot::EpaperRefresh::Full;
        draw(frame);
        const uint8_t* pixels = display.getBuffer();
        Serial.printf("E-paper %s refresh started\n", full ? "full" : "partial");
        if (panelFailed) {
            panel.init(0, false, 5);
        }
        const bool updated = full ? panel.fullRefresh(pixels, redHeader.getBuffer())
                                  : panel.partialRefresh(displayedPixels.data(), pixels);
        const uint32_t finished = millis();
        if (updated) {
            std::copy_n(pixels, displayedPixels.size(), displayedPixels.begin());
            displayed = frame;
            hasBaseline = true;
            panelFailed = false;
            if (full) {
                partials = 0;
                lastFullMs = finished;
            } else {
                ++partials;
            }
            Serial.printf("E-paper %s refresh finished in %lu ms\n", full ? "full" : "partial",
                          static_cast<unsigned long>(finished - now));
        } else {
            Serial.println("E-paper BUSY timeout. Retrying with a full refresh.");
            hasBaseline = false;
            panelFailed = true;
            lastFailureMs = finished;
        }
    }
    delay(20);
}
