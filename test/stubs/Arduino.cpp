#include "Arduino.h"
#include <chrono>
#include <thread>
#include <map>
#include <functional>
#include <cstdlib>

// Dummy setup and loop functions for Arduino framework
void setup() {}
void loop() {}

// Global instances
SerialClass Serial;
TwoWire Wire;

// Pin state tracking
static std::map<uint8_t, uint8_t> pinModes;
static std::map<uint8_t, uint8_t> digitalValues;
static std::map<uint8_t, int> analogValues;
static std::map<uint8_t, std::function<void(void)>> interruptFunctions;
static std::map<uint8_t, std::pair<void(*)(void*), void*>> interruptFunctionsWithArg;
static std::map<uint8_t, int> interruptModes;

// Time tracking
static std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

// Arduino functions implementation
void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

unsigned long millis() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
}

void pinMode(uint8_t pin, uint8_t mode) {
    pinModes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
    digitalValues[pin] = val;
    
    // Check if this pin has an interrupt attached and if the mode matches
    if (interruptFunctions.count(pin) > 0) {
        int mode = interruptModes[pin];
        bool trigger = false;
        
        if (mode == CHANGE) {
            trigger = true;
        } else if (mode == RISING && val == HIGH) {
            trigger = true;
        } else if (mode == FALLING && val == LOW) {
            trigger = true;
        }
        
        if (trigger) {
            interruptFunctions[pin]();
        }
    }
    
    if (interruptFunctionsWithArg.count(pin) > 0) {
        int mode = interruptModes[pin];
        bool trigger = false;
        
        if (mode == CHANGE) {
            trigger = true;
        } else if (mode == RISING && val == HIGH) {
            trigger = true;
        } else if (mode == FALLING && val == LOW) {
            trigger = true;
        }
        
        if (trigger) {
            auto& func = interruptFunctionsWithArg[pin];
            func.first(func.second);
        }
    }
}

int digitalRead(uint8_t pin) {
    return digitalValues.count(pin) ? digitalValues[pin] : LOW;
}

int analogRead(uint8_t pin) {
    return analogValues.count(pin) ? analogValues[pin] : 0;
}

void analogWrite(uint8_t pin, int val) {
    analogValues[pin] = val;
}

void attachInterrupt(uint8_t pin, void (*func)(void), int mode) {
    interruptFunctions[pin] = func;
    interruptModes[pin] = mode;
}

void attachInterruptArg(uint8_t pin, void (*func)(void*), void* arg, int mode) {
    interruptFunctionsWithArg[pin] = std::make_pair(func, arg);
    interruptModes[pin] = mode;
}

void detachInterrupt(uint8_t pin) {
    interruptFunctions.erase(pin);
    interruptFunctionsWithArg.erase(pin);
    interruptModes.erase(pin);
}

uint8_t digitalPinToInterrupt(uint8_t pin) {
    return pin; // In our stub, we just return the pin as is
}

// FreeRTOS stubs implementation
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize) {
    return malloc(uxQueueLength * uxItemSize); // Simple allocation to return a non-null pointer
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
    return pdTRUE; // Always succeed in tests
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void* pvItemToQueue, BaseType_t* pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return pdTRUE; // Always succeed in tests
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    return pdFALSE; // Always fail in tests unless overridden
}

void portYIELD_FROM_ISR(BaseType_t xHigherPriorityTaskWoken) {
    // Do nothing in tests
}

BaseType_t xTaskCreate(void (*pxTaskCode)(void*), const char* pcName, uint32_t usStackDepth, void* pvParameters, uint32_t uxPriority, TaskHandle_t* pxCreatedTask) {
    // In a real test, we might want to actually run the task function
    // For now, just return success
    return pdPASS;
}
