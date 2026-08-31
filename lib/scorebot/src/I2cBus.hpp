#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// The ESP32 Wire implementation and the attached display/input drivers share
// one bus. All task-context transactions use this guard; ISR handlers only
// enqueue work and never touch I2C.
class I2cBus final {
public:
    static bool initialize();

    class Guard final {
    public:
        Guard();
        ~Guard();
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        bool locked;
    };

private:
    static SemaphoreHandle_t mutex;
};
