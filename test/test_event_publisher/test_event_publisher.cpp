#include <unity.h>
#include <EventPublisher.hpp>
#include <Event.hpp>

// Test setup function
void setUp(void) {
    // This function will be called before each test
}

// Test teardown function
void tearDown(void) {
    // This function will be called after each test
}

// Test that EventPublisher can publish events
void test_event_publisher_publish() {
    // Create a queue
    QueueHandle_t queue = xQueueCreate(10, sizeof(Event));
    
    // Create EventPublisher
    EventPublisher publisher(queue);
    
    // Create an event
    Event event{};
    event.type = EventType::ButtonPressed;
    
    // Publish the event
    bool result = publisher.publishEvent(event);
    
    // Verify the result
    TEST_ASSERT_TRUE(result);
    
    // Clean up
    free(queue);
}

// Test that EventPublisher can publish events from ISR
void test_event_publisher_publish_from_isr() {
    // Create a queue
    QueueHandle_t queue = xQueueCreate(10, sizeof(Event));
    
    // Create EventPublisher
    EventPublisher publisher(queue);
    
    // Create an event
    Event event{};
    event.type = EventType::ButtonPressed;
    
    // Publish the event from ISR
    bool result = publisher.publishEventFromISR(event);
    
    // Verify the result
    TEST_ASSERT_TRUE(result);
    
    // Clean up
    free(queue);
}

// Main function required by PlatformIO Unity test framework
int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_event_publisher_publish);
    RUN_TEST(test_event_publisher_publish_from_isr);
    
    return UNITY_END();
}
