#include <RotaryEncoder.hpp>
#include <Coordinator.hpp>

void IRAM_ATTR rotaryEncoderISR(void* arg) {
    RotaryEncoder* self = (RotaryEncoder*)arg;
    Event event;
    event.type = EventType::ButtonPressed;
    BaseType_t higherPriorityWoken = pdFALSE;
    xQueueSendFromISR(self->coordinator->eventQueue, &event, &higherPriorityWoken);
    portYIELD_FROM_ISR(higherPriorityWoken);
}