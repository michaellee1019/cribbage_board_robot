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

    local sources=(
        test/native/test_error_handler.cpp
        test/native/test_game_rules.cpp
        test/native/test_player_ui_rules.cpp
        test/native/test_leaderboard_ui_rules.cpp
        test/native/test_light_color_rules.cpp
        test/native/test_replication_rules.cpp
        test/native/test_board_identity.cpp
        test/native/test_button_input_rules.cpp
        test/native/test_ota_transfer_rules.cpp
        test/native/test_protocol.cpp
        test/native/test_display_brightness.cpp
        test/native/test_visual_feedback_rules.cpp
        test/native/test_ble_power_rules.cpp
        test/native/test_sleep_rules.cpp
        test/native/test_usb_connection_rules.cpp
        test/native/test_message_authority_rules.cpp
    )
    local source binary
    for source in "${sources[@]}"; do
        binary="$test_dir/$(basename "${source%.cpp}")"
        local extra_sources=()
        if [[ "$source" == "test/native/test_board_identity.cpp" ]]; then
            extra_sources+=(src/BoardRole.cpp)
        fi
        g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
            -I. -Ilib/scorebot/src "$source" "${extra_sources[@]}" -o "$binary"
        "$binary"
    done
    python3 test/test_usb_port.py
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
