#!/bin/bash

# Test runner script for Scorebot
# Makes it easy to run different types of tests

set -e

echo "🧪 Scorebot Test Runner"
echo "=================================="

run_logic_tests() {
    echo "🧪 Running native business-logic tests (No Hardware Required)"
    echo "========================================================="
    local test_dir
    test_dir=$(mktemp -d)
    trap 'rm -rf "$test_dir"' RETURN

    g++ -std=c++17 -I. test/test_error_handler.cpp -o "$test_dir/error_handler"
    "$test_dir/error_handler"
    g++ -std=c++17 -Ilib/scorebot/src test/test_game_rules.cpp -o "$test_dir/game_rules"
    "$test_dir/game_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_replication_rules.cpp -o "$test_dir/replication_rules"
    "$test_dir/replication_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_ota_transfer_rules.cpp -o "$test_dir/ota_transfer_rules"
    "$test_dir/ota_transfer_rules"
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
