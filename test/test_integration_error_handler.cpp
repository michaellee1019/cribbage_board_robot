#include <unity.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "ErrorHandler.hpp"

// Integration tests that run on actual hardware
// These test real FreeRTOS interactions

void test_queue_creation_success() {
    // Test successful queue creation
    QueueHandle_t testQueue = xQueueCreate(5, sizeof(int));
    
    bool result = ErrorHandler::checkPointer(testQueue, ErrorCode::QUEUE_CREATE_FAILED, "integration test queue");
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_NOT_NULL(testQueue);
    
    // Clean up
    if (testQueue) {
        vQueueDelete(testQueue);
    }
}

void test_task_creation_success() {
    // Simple test task
    auto testTask = [](void* param) {
        vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelete(nullptr);
    };
    
    TaskHandle_t taskHandle = nullptr;
    BaseType_t result = xTaskCreate(testTask, "test_task", 1024, nullptr, 1, &taskHandle);
    
    bool checkResult = ErrorHandler::checkFreeRTOSResult(result, ErrorCode::TASK_CREATE_FAILED, "integration test task");
    
    TEST_ASSERT_TRUE(checkResult);
    TEST_ASSERT_EQUAL(pdPASS, result);
    TEST_ASSERT_NOT_NULL(taskHandle);
    
    // Task will self-delete after 10ms
    vTaskDelay(pdMS_TO_TICKS(20));
}

void test_memory_allocation_patterns() {
    // Test typical memory allocation patterns
    void* ptr1 = malloc(100);
    bool result1 = ErrorHandler::checkPointer(ptr1, ErrorCode::MEMORY_ALLOCATION_FAILED, "test allocation 1");
    TEST_ASSERT_TRUE(result1);
    free(ptr1);
    
    void* ptr2 = malloc(1000);
    bool result2 = ErrorHandler::checkPointer(ptr2, ErrorCode::MEMORY_ALLOCATION_FAILED, "test allocation 2");
    TEST_ASSERT_TRUE(result2);
    free(ptr2);
}

void test_freertos_resource_limits() {
    // Test behavior near resource limits
    // Create multiple queues to test resource management
    QueueHandle_t queues[10];
    int created_count = 0;
    
    for (int i = 0; i < 10; i++) {
        queues[i] = xQueueCreate(2, sizeof(int));
        if (queues[i] != nullptr) {
            created_count++;
        }
    }
    
    TEST_ASSERT_GREATER_THAN(0, created_count);
    
    // Clean up
    for (int i = 0; i < created_count; i++) {
        if (queues[i]) {
            vQueueDelete(queues[i]);
        }
    }
}

void test_error_handler_thread_safety() {
    // Test that error handler can be called from multiple contexts
    // This is important for ISR safety
    
    struct TestContext {
        bool success;
        SemaphoreHandle_t semaphore;
    };
    
    TestContext context = {false, nullptr};
    context.semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(context.semaphore);
    
    auto testTask = [](void* param) {
        TestContext* ctx = static_cast<TestContext*>(param);
        
        // Test error handler from task context
        int dummy = 42;
        ctx->success = ErrorHandler::checkPointer(&dummy, ErrorCode::MEMORY_ALLOCATION_FAILED, "thread safety test");
        
        xSemaphoreGive(ctx->semaphore);
        vTaskDelete(nullptr);
    };
    
    TaskHandle_t taskHandle = nullptr;
    BaseType_t result = xTaskCreate(testTask, "thread_test", 2048, &context, 1, &taskHandle);
    TEST_ASSERT_EQUAL(pdPASS, result);
    
    // Wait for task completion
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(context.semaphore, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_TRUE(context.success);
    
    vSemaphoreDelete(context.semaphore);
}

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
    vTaskDelay(pdMS_TO_TICKS(10)); // Allow cleanup
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_queue_creation_success);
    RUN_TEST(test_task_creation_success);
    RUN_TEST(test_memory_allocation_patterns);
    RUN_TEST(test_freertos_resource_limits);
    RUN_TEST(test_error_handler_thread_safety);
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}