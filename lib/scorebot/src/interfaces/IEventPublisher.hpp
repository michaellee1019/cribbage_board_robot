#ifndef IEVENT_PUBLISHER_H
#define IEVENT_PUBLISHER_H

#include "../Event.hpp"

/**
 * @brief Interface for publishing events
 * 
 * This abstraction allows components to publish events without
 * directly depending on the Coordinator or FreeRTOS queues
 */
class IEventPublisher {
public:
    virtual ~IEventPublisher() = default;
    
    /**
     * @brief Publish an event
     * 
     * @param event The event to publish
     * @return true if the event was successfully published
     */
    virtual bool publishEvent(const Event& event) = 0;
    
    /**
     * @brief Publish an event from an ISR context
     * 
     * @param event The event to publish
     * @return true if the event was successfully published
     */
    virtual bool publishEventFromISR(const Event& event) = 0;
};

#endif // IEVENT_PUBLISHER_H
