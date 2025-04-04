#ifndef LIGHT_FADE_H
#define LIGHT_FADE_H

#include <Light.hpp>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


class LightFade {
    static constexpr EventBits_t ENABLED  = BIT0;
    static constexpr EventBits_t DISABLED = BIT1;

    Light& light;
    bool fading;
    EventGroupHandle_t enableEvent;

public:
    explicit LightFade(Light& light)
        : light{light}, fading{false}, enableEvent{xEventGroupCreate()}
    {}

    void setup() {
        xTaskCreate(
            &LightFade::blinkTask,
            "fadeTask",
            2048,
            this,
            5,
            nullptr
        );
    }

    void blinkEnabled() {
        fading = true;
        xEventGroupSetBits(enableEvent, ENABLED);
    }

    void blinkDisabled() {
        fading = false;
        xEventGroupSetBits(enableEvent, DISABLED);
    }

private:
    static constexpr int UPDATES_PER_SECOND = 15;
    static constexpr int FADE_DELTA = 100 / UPDATES_PER_SECOND;
    static constexpr TickType_t DELAY_TICKS = pdMS_TO_TICKS(1000 / UPDATES_PER_SECOND);


    [[noreturn]]
    static void blinkTask(void* pvParameter) {
        auto controller = static_cast<LightFade*>(pvParameter);

        int brightness = 0;
        bool fadingIn = true;

        while (true) {
            EventBits_t bits = xEventGroupWaitBits(
                controller->enableEvent,
                ENABLED | DISABLED,
                pdTRUE /*clear on exit*/,
                pdFALSE /*wait for all bits*/,
                0 /* ticks to wait*/
            );

            if (bits & ENABLED) {
                controller->fading = true;
            }
            else if (bits & DISABLED) {
                controller->fading = false;
                controller->light.setBrightness(0);
                brightness = 0;
                fadingIn = true;
            }

            if (controller->fading) {
                controller->light.setBrightness(brightness);

                if (fadingIn) {
                    brightness += FADE_DELTA;
                    if (brightness >= 100) {
                        brightness = 100;
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