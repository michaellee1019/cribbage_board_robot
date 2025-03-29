#include <ButtonGrid.hpp>
#include <Coordinator.hpp>
#include <Event.hpp>

void IRAM_ATTR buttonISR(void* arg) {
    ButtonGrid* self = (ButtonGrid*)arg;
    Event event;
    event.type = EventType::ButtonPressed;
    BaseType_t higherPriorityWoken = pdFALSE;
    xQueueSendFromISR(self->coordinator->eventQueue, &event, &higherPriorityWoken);
    portYIELD_FROM_ISR(higherPriorityWoken);
}