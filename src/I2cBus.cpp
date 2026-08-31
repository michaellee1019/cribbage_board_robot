#include <I2cBus.hpp>

SemaphoreHandle_t I2cBus::mutex = nullptr;

bool I2cBus::initialize() {
    if (mutex == nullptr) {
        mutex = xSemaphoreCreateMutex();
    }
    return mutex != nullptr;
}

I2cBus::Guard::Guard() : locked(I2cBus::mutex != nullptr) {
    if (locked) {
        xSemaphoreTake(I2cBus::mutex, portMAX_DELAY);
    }
}

I2cBus::Guard::~Guard() {
    if (locked) {
        xSemaphoreGive(I2cBus::mutex);
    }
}
