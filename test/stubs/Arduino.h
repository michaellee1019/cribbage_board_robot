#ifndef ARDUINO_STUB_H
#define ARDUINO_STUB_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Arduino pin modes
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

// Arduino pin states
#define HIGH 0x1
#define LOW 0x0

// Arduino interrupt modes
#define CHANGE 0x1
#define RISING 0x2
#define FALLING 0x3

// Mock Serial class
class SerialClass {
public:
    void begin(unsigned long baud) {}
    void print(const char* str) {}
    void print(int val) {}
    void print(float val) {}
    void println(const char* str) {}
    void println(int val) {}
    void println(float val) {}
    void println() {}
};

extern SerialClass Serial;

// Mock Wire class
class TwoWire {
public:
    void begin() {}
    void begin(int sda, int scl) {}
    void setClock(uint32_t clock) {}
    void beginTransmission(uint8_t address) {}
    uint8_t endTransmission(bool sendStop = true) { return 0; }
    uint8_t requestFrom(uint8_t address, uint8_t quantity, bool sendStop = true) { return quantity; }
    size_t write(uint8_t data) { return 1; }
    size_t write(const uint8_t* data, size_t quantity) { return quantity; }
    int available() { return 0; }
    int read() { return 0; }
};

extern TwoWire Wire;

// Arduino functions
void delay(unsigned long ms);
unsigned long millis();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
void attachInterrupt(uint8_t pin, void (*func)(void), int mode);
void attachInterruptArg(uint8_t pin, void (*func)(void*), void* arg, int mode);
void detachInterrupt(uint8_t pin);
uint8_t digitalPinToInterrupt(uint8_t pin);

// FreeRTOS stubs
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;

#define portMAX_DELAY 0xFFFFFFFF
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1

#define IRAM_ATTR

QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void* pvItemToQueue, BaseType_t* pxHigherPriorityTaskWoken);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);
void portYIELD_FROM_ISR(BaseType_t xHigherPriorityTaskWoken);
BaseType_t xTaskCreate(void (*pxTaskCode)(void*), const char* pcName, uint32_t usStackDepth, void* pvParameters, uint32_t uxPriority, TaskHandle_t* pxCreatedTask);

// Scheduler stub for painlessMesh
class Scheduler {
public:
    Scheduler() {}
};

#endif // ARDUINO_STUB_H
