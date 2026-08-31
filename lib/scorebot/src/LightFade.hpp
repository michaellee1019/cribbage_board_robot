#ifndef LIGHT_FADE_H
#define LIGHT_FADE_H

#include <Light.hpp>
#include <ErrorHandler.hpp>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


class LightFade {
    static constexpr EventBits_t B_ENABLED  = BIT0;
    static constexpr EventBits_t B_DISABLED = BIT1;

    Light& light;
    EventGroupHandle_t enableEvent;

public:
    explicit LightFade(Light& light)
        : light{light}, enableEvent{xEventGroupCreate()}
    {
        CHECK_POINTER(enableEvent, ErrorCode::EVENT_GROUP_CREATE_FAILED, "LightFade event group");
    }

    void setup() {
        BaseType_t taskResult = xTaskCreate(
            &LightFade::blinkTask,
            "fadeTask",
            2048,
            this,
            5,
            nullptr
        );
        CHECK_FREERTOS_RESULT(taskResult, ErrorCode::TASK_CREATE_FAILED, "LightFade blink task");
    }

    void blinkEnabled() {
        xEventGroupSetBits(enableEvent, B_ENABLED);
    }

    void blinkDisabled() {
        xEventGroupSetBits(enableEvent, B_DISABLED);
    }

private:
    static constexpr int UPDATES_PER_SECOND = 15; // Is this too many? Perhaps it's maxing out the i2c IO capacity. Scope it?
    static constexpr int MAX_BRIGHTNESS = 50;  // 50% brightness for player turn
    static constexpr int FADE_DELTA = MAX_BRIGHTNESS / UPDATES_PER_SECOND;
    static constexpr TickType_t DELAY_TICKS = pdMS_TO_TICKS(1000 / UPDATES_PER_SECOND);


    [[noreturn]]
    static void blinkTask(void* pvParameter) {
        auto controller = static_cast<LightFade*>(pvParameter);

        int brightness = 0;
        bool fadingIn = true;
        bool fading = false;

        while (true) {
            EventBits_t bits = xEventGroupWaitBits(
                controller->enableEvent,
                B_ENABLED | B_DISABLED,
                pdTRUE /*clear on exit*/,
                pdFALSE /*wait for all bits*/,
                fading ? 0 : portMAX_DELAY
            );

            if (bits & B_ENABLED) {
                fading = true;
            }
            if (bits & B_DISABLED) {
                fading = false;
                controller->light.setBrightness(0);
                brightness = 0;
                fadingIn = true;
            }

            if (fading) {
                controller->light.setBrightness(brightness);

                if (fadingIn) {
                    brightness += FADE_DELTA;
                    if (brightness >= MAX_BRIGHTNESS) {
                        brightness = MAX_BRIGHTNESS;
                        fadingIn = false;
                    }
                } else {
                    brightness -= FADE_DELTA;
                    if (brightness <= 0) {
                        brightness = 0;
                        fadingIn = true;
                    }
                }
            }

            vTaskDelay(DELAY_TICKS);
        }
    }
};


#endif
