#ifndef RTBUTTON_H
#define RTBUTTON_H

#include <Arduino.h>
#include <ErrorHandler.hpp>
#include <utils.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

class RTButton {
public:
    RTButton(int pin, int debounceDelay, int doubleClickThreshold)
        : buttonPin(pin),
          debounceDelayMs(debounceDelay),
          doubleClickThresholdMs(doubleClickThreshold),
          lastInterruptTime(0) {
        buttonSemaphore = xSemaphoreCreateBinary();
        CHECK_POINTER(buttonSemaphore, ErrorCode::SEMAPHORE_CREATE_FAILED, "RTButton semaphore");

        debounceTimer = xTimerCreate("Debounce Timer",
                                     pdMS_TO_TICKS(debounceDelayMs),
                                     pdFALSE,  // One-shot timer
                                     this,
                                     [](TimerHandle_t xTimer) {
                                         auto* button =
                                             static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                                         if (button)
                                             button->debounceCallback();
                                     });

        doubleClickTimer = xTimerCreate("Double-Click Timer",
                                        pdMS_TO_TICKS(doubleClickThresholdMs),
                                        pdFALSE,  // One-shot timer
                                        this,
                                        [](TimerHandle_t xTimer) {
                                            auto* button =
                                                static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                                            if (button)
                                                button->doubleClickCallback();
                                        });

        CHECK_POINTER(debounceTimer, ErrorCode::TIMER_CREATE_FAILED, "RTButton debounce timer");
        CHECK_POINTER(doubleClickTimer, ErrorCode::TIMER_CREATE_FAILED, "RTButton double-click timer");
    }

    RTButton(const RTButton&) = delete;
    RTButton& operator=(const RTButton&) = delete;


    virtual ~RTButton() {
        if (debounceTimer != nullptr) {
            xTimerDelete(debounceTimer, portMAX_DELAY);
        }
        if (doubleClickTimer != nullptr) {
            xTimerDelete(doubleClickTimer, portMAX_DELAY);
        }
        if (buttonSemaphore != nullptr) {
            vSemaphoreDelete(buttonSemaphore);
        }
    }

    void init() {
        pinMode(buttonPin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(buttonPin), ISRHandler, this, CHANGE);

        BaseType_t taskCreated = xTaskCreate(
            [](void* param) {
                auto* button = static_cast<RTButton*>(param);
                if (button)
                    button->buttonTask();
            },
            "Button Task",
            2048,
            this,
            1,
            nullptr);

        CHECK_FREERTOS_RESULT(taskCreated, ErrorCode::TASK_CREATE_FAILED, "RTButton task");
    }

protected:
    virtual void onPress() {
        DEBUG_PRINTLN("Button Pressed");
    }

    virtual void onRelease() {
        DEBUG_PRINTLN("Button Released");
    }

    virtual void onSingleClick() {
        DEBUG_PRINTLN("Single Click Detected");
    }

    virtual void onDoubleClick() {
        DEBUG_PRINTLN("Double Click Detected");
    }

private:
    const unsigned short buttonPin;
    const unsigned short debounceDelayMs;
    const unsigned short doubleClickThresholdMs;

    SemaphoreHandle_t buttonSemaphore;
    TimerHandle_t debounceTimer;
    TimerHandle_t doubleClickTimer;

    volatile bool buttonPressed = false;
    volatile bool isDoubleClick = false;
    volatile size_t clickCount = 0;
    volatile TickType_t lastInterruptTime = 0;

    static void IRAM_ATTR ISRHandler(void* arg) {
        auto* button = static_cast<RTButton*>(arg);
        if (button)
            button->handleInterrupt();
    }

    void handleInterrupt() {
        TickType_t interruptTime = xTaskGetTickCountFromISR();

        if ((interruptTime - lastInterruptTime) * portTICK_PERIOD_MS > debounceDelayMs) {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xTimerResetFromISR(debounceTimer, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        lastInterruptTime = interruptTime;
    }

    void debounceCallback() {
        if (digitalRead(buttonPin) == LOW) {
            buttonPressed = true;
            xSemaphoreGive(buttonSemaphore);
        } else {
            buttonPressed = false;
            xSemaphoreGive(buttonSemaphore);
        }
    }

    void doubleClickCallback() {
        if (clickCount == 1) {
            onSingleClick();
        } else if (clickCount == 2) {
            isDoubleClick = true;
            onDoubleClick();
        }
        clickCount = 0;
    }

    [[noreturn]]
    void buttonTask() {
        while (true) {
            if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
                if (buttonPressed) {
                    onPress();
                    clickCount++;
                    xTimerStart(doubleClickTimer, 0);
                } else {
                    onRelease();
                }
            }
        }
    }
};


#endif
