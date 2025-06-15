#ifndef EVENT_PUBLISHER_H
#define EVENT_PUBLISHER_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "interfaces/IEventPublisher.hpp"

/**
 * @brief Concrete implementation of IEventPublisher using FreeRTOS queues
 */
class EventPublisher : public IEventPublisher {
private:
    QueueHandle_t eventQueue;

public:
    explicit EventPublisher(QueueHandle_t eventQueue) : eventQueue(eventQueue) {}

    bool publishEvent(const Event& event) override {
        return xQueueSend(eventQueue, &event, portMAX_DELAY) == pdTRUE;
    }

    bool publishEventFromISR(const Event& event) override {
        BaseType_t higherPriorityWoken = pdFALSE;
        bool result = xQueueSendFromISR(eventQueue, &event, &higherPriorityWoken) == pdTRUE;
        portYIELD_FROM_ISR(higherPriorityWoken);
        return result;
    }
};

#endif // EVENT_PUBLISHER_H
