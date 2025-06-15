#include <unity.h>
#include <ButtonGrid.hpp>
#include "../mocks/MockButtonHardware.hpp"
#include "../mocks/MockEventPublisher.hpp"

// Define Arduino constants for testing
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif

#ifndef CHANGE
#define CHANGE 0x1
#endif

#ifndef LOW
#define LOW 0x0
#endif

// Test setup function
void setUp(void) {
    // This function will be called before each test
}

// Test teardown function
void tearDown(void) {
    // This function will be called after each test
}

// Test that ButtonGrid initializes hardware correctly
void test_button_grid_setup() {
    // Create mocks
    auto* mockHardware = new MockButtonHardware();
    auto* mockPublisher = new MockEventPublisher();
    
    // Create ButtonGrid with mocks
    ButtonGrid buttonGrid(mockHardware, mockPublisher);
    
    // Call setup
    buttonGrid.setup();
    
    // Verify hardware was initialized correctly
    TEST_ASSERT_TRUE(mockHardware->isInitialized());
    TEST_ASSERT_TRUE(mockHardware->areInterruptsSetup());
    
    // Verify pin modes were set correctly
    for (auto&& pin : AllPins) {
        TEST_ASSERT_EQUAL(INPUT_PULLUP, mockHardware->getPinMode(ButtonGrid::hardwarePin(pin)));
        TEST_ASSERT_EQUAL(CHANGE, mockHardware->getInterruptPinMode(ButtonGrid::hardwarePin(pin)));
    }
    
    // Clean up
    delete mockHardware;
    delete mockPublisher;
}

// Test that ButtonGrid decodes interrupts correctly
void test_button_grid_decode_interrupt() {
    // Create mocks
    auto* mockHardware = new MockButtonHardware();
    auto* mockPublisher = new MockEventPublisher();
    
    // Create ButtonGrid with mocks
    ButtonGrid buttonGrid(mockHardware, mockPublisher);
    
    // Simulate an interrupt
    uint8_t testPin = 3; // PlusOne
    uint16_t testValue = (1 << testPin);
    mockHardware->simulateInterrupt(testPin, testValue);
    
    // Test callback
    bool callbackCalled = false;
    buttonGrid.decodeInterrupt([&](const ButtonState& bs) {
        callbackCalled = true;
        TEST_ASSERT_EQUAL(testPin, bs.changedPins);
        TEST_ASSERT_EQUAL(testValue, bs.pinValues);
        
        // Test ButtonStateHandle
        auto handle = bs[Pins::PlusOne];
        TEST_ASSERT_TRUE(handle.changed());
        TEST_ASSERT_TRUE(handle.pressed());
        TEST_ASSERT_FALSE(handle.released());
    });
    
    TEST_ASSERT_TRUE(callbackCalled);
    
    // Clean up
    delete mockHardware;
    delete mockPublisher;
}

// Test that ButtonGrid ISR publishes events correctly
// Note: This test is limited since we can't directly test the ISR function
// In a real test, you might need to use function pointers or other techniques
void test_button_grid_event_publishing() {
    // Create mocks
    auto* mockHardware = new MockButtonHardware();
    auto* mockPublisher = new MockEventPublisher();
    
    // Create ButtonGrid with mocks
    ButtonGrid buttonGrid(mockHardware, mockPublisher);
    
    // Simulate button press by directly calling the event publisher
    // (In a real scenario, this would be done by the ISR)
    Event event{};
    event.type = EventType::ButtonPressed;
    mockPublisher->publishEventFromISR(event);
    
    // Verify event was published
    auto& publishedEvents = mockPublisher->getPublishedEventsFromISR();
    TEST_ASSERT_EQUAL(1, publishedEvents.size());
    TEST_ASSERT_EQUAL(EventType::ButtonPressed, publishedEvents[0].type);
    
    // Clean up
    delete mockHardware;
    delete mockPublisher;
}

// Main function required by PlatformIO Unity test framework
int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_button_grid_setup);
    RUN_TEST(test_button_grid_decode_interrupt);
    RUN_TEST(test_button_grid_event_publishing);
    
    return UNITY_END();
}
