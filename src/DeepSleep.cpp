#include <DeepSleep.hpp>

#include <Coordinator.hpp>
#include <I2cBus.hpp>
#include <SleepRules.hpp>

#include <Wire.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

namespace {
constexpr uint8_t kRotaryInterrupt = 7;
constexpr uint8_t kButtonInterrupt = 8;
constexpr uint64_t kInputWakeMask =
    (1ULL << kRotaryInterrupt) | (1ULL << kButtonInterrupt);

bool inputAsserted() {
    pinMode(kRotaryInterrupt, INPUT_PULLUP);
    pinMode(kButtonInterrupt, INPUT_PULLUP);
    return digitalRead(kRotaryInterrupt) == LOW ||
           digitalRead(kButtonInterrupt) == LOW;
}

[[noreturn]] void startDeepSleep() {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(
        scorebot::sleep::kStatusPulseIntervalUs));
    // ESP32-S3 hardware interprets EXT1 mode value zero as ANY_LOW. This IDF
    // 4.4 header incorrectly retains the ESP32-only ALL_LOW name for that same
    // value; later IDF versions expose the correct ESP_EXT1_WAKEUP_ANY_LOW
    // symbol. Both interrupt outputs are actively driven, so RTC_PERIPH and its
    // internal pulls can remain off while either line wakes the board directly.
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(
        kInputWakeMask, ESP_EXT1_WAKEUP_ALL_LOW));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF));
    esp_deep_sleep_disable_rom_logging();
    esp_deep_sleep_start();
    __builtin_unreachable();
}
}  // namespace

namespace scorebot::deep_sleep {

void handleTimerWake(Coordinator& coordinator) {
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT1 || cause == ESP_SLEEP_WAKEUP_TIMER) {
        // EXT1 leaves wake pads in RTC-IO mode. Restore ordinary GPIO ownership
        // before checking them or configuring their interrupts. Timer wake also
        // needs this because EXT1 was armed during the same sleep interval.
        ESP_ERROR_CHECK(rtc_gpio_deinit(GPIO_NUM_7));
        ESP_ERROR_CHECK(rtc_gpio_deinit(GPIO_NUM_8));
    }
    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        return;
    }
    if (cause != ESP_SLEEP_WAKEUP_TIMER) {
        return;
    }

    // Check the two externally-driven interrupt lines before initializing I2C.
    // If either is asserted, normal setup owns the bus from the beginning.
    if (inputAsserted()) {
        return;
    }

    Wire.begin(5, 6);
    I2cBus::initialize();

    coordinator.rotaryEncoder.setupStatusPixelOnly();
    coordinator.rotaryEncoder.setColor(0x080808);
    delay(sleep::kStatusPulseMs);
    coordinator.rotaryEncoder.setColor(0x000000);

    if (inputAsserted()) {
        Wire.end();
        return;
    }
    Wire.end();
    startDeepSleep();
}

[[noreturn]] void enter(Coordinator& coordinator) {
    // Stop emitting light before shutting down the I2C devices that own it.
    coordinator.setPlayerTurnAnimation(false);
    coordinator.setLeaderboardTurnColor(0);
    coordinator.rotaryEncoder.setColor(0x000000);
    coordinator.display1.sleep();
    coordinator.display2.sleep();
    coordinator.display3.sleep();
    coordinator.display4.sleep();

    // Detach CPU ISRs but preserve peripheral edge latches. A new press during
    // shutdown keeps its active-low line asserted and causes an immediate GPIO
    // wake instead of being lost.
    coordinator.buttonGrid.prepareForSleep();
    coordinator.rotaryEncoder.prepareForSleep();
    coordinator.ble.shutdownForSleep();
    Wire.end();
    startDeepSleep();
}

}  // namespace scorebot::deep_sleep
