#!/bin/bash

# Test runner script for Cribbage Board Robot
# Makes it easy to run different types of tests

set -e

echo "🧪 Cribbage Board Robot Test Runner"
echo "=================================="

run_logic_tests() {
    echo "🧪 Running ErrorHandler Logic Tests (No Hardware Required)"
    echo "========================================================="
    
    # Create test that uses the actual ErrorHandler.hpp
    cat > test_logic_temp.cpp << 'EOF'
#include <iostream>
#include <cassert>

// Use the actual ErrorHandler by including it with NATIVE_BUILD flag
#define NATIVE_BUILD
#include "lib/scorebot/src/ErrorHandler.hpp"

void test_success_cases() {
    int dummy = 42;
    assert(ErrorHandler::checkFreeRTOSResult(pdPASS, ErrorCode::TASK_CREATE_FAILED, "test") == true);
    assert(ErrorHandler::checkPointer(&dummy, ErrorCode::MEMORY_ALLOCATION_FAILED, "test") == true);
    assert(CHECK_FREERTOS_RESULT(pdPASS, ErrorCode::QUEUE_CREATE_FAILED, "test") == true);
    std::cout << "✓ Success cases work correctly" << std::endl;
}

void test_failure_cases() {
    // Note: In native mode, these don't actually restart - they just return false
    assert(ErrorHandler::checkFreeRTOSResult(pdFAIL, ErrorCode::TASK_CREATE_FAILED, "test") == false);
    assert(ErrorHandler::checkPointer(nullptr, ErrorCode::MEMORY_ALLOCATION_FAILED, "test") == false);
    std::cout << "✓ Failure cases work correctly" << std::endl;
}

void test_error_codes() {
    assert(static_cast<int>(ErrorCode::QUEUE_CREATE_FAILED) == 0);
    assert(static_cast<int>(ErrorCode::TASK_CREATE_FAILED) == 1);
    std::cout << "✓ Error codes are consistent" << std::endl;
}

int main() {
    test_success_cases();
    test_failure_cases(); 
    test_error_codes();
    std::cout << "\n✅ All ErrorHandler logic tests passed!" << std::endl;
    return 0;
}
EOF

    # Compile and run the test that uses the actual ErrorHandler
    if g++ -std=c++17 -I. test_logic_temp.cpp -o test_logic_temp 2>/dev/null; then
        ./test_logic_temp
        echo ""
    else
        echo "❌ Failed to compile logic tests"
        echo "This may indicate the platform abstraction needs work"
        return 1
    fi
    
    # Cleanup
    rm -f test_logic_temp.cpp test_logic_temp
}

case "${1:-embedded}" in
    "logic"|"native")
        run_logic_tests
        ;;
    "embedded"|"integration")
        echo "Running integration tests (requires ESP32 hardware)..."
        pio test -e test_embedded
        ;;
    "error-handler")
        echo "Running error handler tests..."
        echo "📋 Logic tests (no hardware):"
        run_logic_tests
        echo "🔌 Integration tests (requires ESP32):"
        pio test -e test_embedded -f test_integration_error_handler
        ;;
    "all")
        echo "Running all tests..."
        echo "📋 Logic tests (no hardware):"
        run_logic_tests
        echo "🔌 Integration tests (requires ESP32):"
        pio test -e test_embedded
        echo "✅ All tests completed!"
        ;;
    *)
        echo "Running integration tests (requires ESP32 hardware)..."
        pio test -e test_embedded
        ;;
esac

echo ""
echo "Test run complete! 🎉"