#if !defined(ARDUINO)

#include <cassert>
#include <iostream>

#define NATIVE_BUILD
#include <ErrorHandler.hpp>

void testSuccessCases() {
    int value = 42;
    assert(ErrorHandler::checkFreeRTOSResult(pdPASS, ErrorCode::TASK_CREATE_FAILED, "test"));
    assert(ErrorHandler::checkPointer(&value, ErrorCode::MEMORY_ALLOCATION_FAILED, "test"));
    assert(CHECK_FREERTOS_RESULT(pdPASS, ErrorCode::QUEUE_CREATE_FAILED, "test"));
}

void testFailureCases() {
    assert(!ErrorHandler::checkFreeRTOSResult(pdFAIL, ErrorCode::TASK_CREATE_FAILED, "test"));
    assert(!ErrorHandler::checkPointer(nullptr, ErrorCode::MEMORY_ALLOCATION_FAILED, "test"));
}

int main() {
    testSuccessCases();
    testFailureCases();
    std::cout << "Error-handler tests passed\n";
}

#endif
