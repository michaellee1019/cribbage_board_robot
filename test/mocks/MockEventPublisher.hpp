#ifndef MOCK_EVENT_PUBLISHER_H
#define MOCK_EVENT_PUBLISHER_H

#include <vector>
#include "../../lib/scorebot/src/interfaces/IEventPublisher.hpp"

/**
 * @brief Mock implementation of IEventPublisher for testing
 */
class MockEventPublisher : public IEventPublisher {
private:
    std::vector<Event> publishedEvents;
    std::vector<Event> publishedEventsFromISR;
    bool shouldFailPublish = false;
    bool shouldFailPublishFromISR = false;

public:
    bool publishEvent(const Event& event) override {
        if (shouldFailPublish) {
            return false;
        }
        publishedEvents.push_back(event);
        return true;
    }
    
    bool publishEventFromISR(const Event& event) override {
        if (shouldFailPublishFromISR) {
            return false;
        }
        publishedEventsFromISR.push_back(event);
        return true;
    }
    
    // Test helper methods
    const std::vector<Event>& getPublishedEvents() const {
        return publishedEvents;
    }
    
    const std::vector<Event>& getPublishedEventsFromISR() const {
        return publishedEventsFromISR;
    }
    
    void clearEvents() {
        publishedEvents.clear();
        publishedEventsFromISR.clear();
    }
    
    void setShouldFailPublish(bool shouldFail) {
        shouldFailPublish = shouldFail;
    }
    
    void setShouldFailPublishFromISR(bool shouldFail) {
        shouldFailPublishFromISR = shouldFail;
    }
};

#endif // MOCK_EVENT_PUBLISHER_H
