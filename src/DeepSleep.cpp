#include <DeepSleep.hpp>

#include <Coordinator.hpp>
#include <I2cBus.hpp>
#include <PlayerBoard.hpp>
#include <SleepRules.hpp>

#include <Wire.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

namespace {
constexpr uint8_t kRotaryInterrupt = 7;
constexpr uint8_t kButtonInterrupt = 8;
constexpr uint64_t kLegacyWakeMask =
    (1ULL << kRotaryInterrupt) | (1ULL << kButtonInterrupt);
constexpr gpio_num_t kPlayerWakePins[] = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
};

bool inputAsserted(bool playerBoard) {
    if (playerBoard) {
        for (const gpio_num_t pin : kPlayerWakePins) {
            pinMode(pin, INPUT_PULLUP);
            if (digitalRead(pin) == LOW) {
                return true;
            }
        }
        return false;
    }
    pinMode(kRotaryInterrupt, INPUT_PULLUP);
    pinMode(kButtonInterrupt, INPUT_PULLUP);
    return digitalRead(kRotaryInterrupt) == LOW ||
           digitalRead(kButtonInterrupt) == LOW;
}

void restoreWakePins(bool playerBoard) {
    if (playerBoard) {
        for (const gpio_num_t pin : kPlayerWakePins) {
            ESP_ERROR_CHECK(rtc_gpio_deinit(pin));
        }
        return;
    }
    ESP_ERROR_CHECK(rtc_gpio_deinit(GPIO_NUM_7));
    ESP_ERROR_CHECK(rtc_gpio_deinit(GPIO_NUM_8));
}

[[noreturn]] void startDeepSleep(bool playerBoard) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(
        scorebot::sleep::kStatusPulseIntervalUs));
    // ESP32-S3 hardware interprets EXT1 mode value zero as ANY_LOW. This IDF
    // 4.4 header incorrectly retains the ESP32-only ALL_LOW name for that same
    // value; later IDF versions expose the correct ESP_EXT1_WAKEUP_ANY_LOW
    // symbol.
    const uint64_t wakeMask = playerBoard
                                  ? scorebot::player_pins::kWakeMask
                                  : kLegacyWakeMask;
    if (playerBoard) {
        // Buttons and the encoder are passive contacts with external pull-ups.
        // The RTC domain has to stay up to see one of those lines go low.
        for (const gpio_num_t pin : kPlayerWakePins) {
            rtc_gpio_init(pin);
            rtc_gpio_set_direction(pin, RTC_GPIO_MODE_INPUT_ONLY);
            rtc_gpio_pulldown_dis(pin);
            rtc_gpio_pullup_en(pin);
        }
        ESP_ERROR_CHECK(esp_sleep_pd_config(
            ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));
    } else {
        // The leaderboard interrupt outputs are actively driven, so RTC_PERIPH
        // and its internal pulls can remain off.
        ESP_ERROR_CHECK(esp_sleep_pd_config(
            ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF));
    }
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(
        wakeMask, ESP_EXT1_WAKEUP_ALL_LOW));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF));
    esp_deep_sleep_disable_rom_logging();
    esp_deep_sleep_start();
    __builtin_unreachable();
}
}  // namespace

namespace scorebot::deep_sleep {

void handleTimerWake(Coordinator& coordinator) {
    const bool playerBoard = scorebot::usesPlayerBoardHardware();
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT1 || cause == ESP_SLEEP_WAKEUP_TIMER) {
        // EXT1 leaves wake pads in RTC-IO mode. Restore ordinary GPIO ownership
        // before checking them or configuring their interrupts. Timer wake also
        // needs this because EXT1 was armed during the same sleep interval.
        restoreWakePins(playerBoard);
    }
    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        return;
    }
    if (cause != ESP_SLEEP_WAKEUP_TIMER) {
        return;
    }

    // A held button or encoder contact owns the rest of startup. The pulse
    // path must not claim those pins or the bus first.
    if (inputAsserted(playerBoard)) {
        return;
    }

    if (playerBoard) {
        scorebot::enablePlayerBoost();
        delay(20);
        coordinator.rotaryEncoder.setupStatusPixelOnly();
        coordinator.rotaryEncoder.setColor(0x080808);
        delay(sleep::kStatusPulseMs);
        coordinator.rotaryEncoder.setColor(0x000000);
        if (inputAsserted(playerBoard)) {
            return;
        }
        scorebot::disablePlayerBoost();
        startDeepSleep(true);
    }

    Wire.begin(5, 6);
    I2cBus::initialize();

    coordinator.rotaryEncoder.setupStatusPixelOnly();
    coordinator.rotaryEncoder.setColor(0x080808);
    delay(sleep::kStatusPulseMs);
    coordinator.rotaryEncoder.setColor(0x000000);

    if (inputAsserted(false)) {
        Wire.end();
        return;
    }
    Wire.end();
    startDeepSleep(false);
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
    const bool playerBoard = scorebot::usesPlayerBoardHardware();
    if (playerBoard) {
        // The 10 kΩ pulldown on the power board holds the boost off once this
        // GPIO is no longer driving it. The display and side LEDs are on that
        // rail, so they drop with it.
        scorebot::disablePlayerBoost();
    }
    startDeepSleep(playerBoard);
}

}  // namespace scorebot::deep_sleep
