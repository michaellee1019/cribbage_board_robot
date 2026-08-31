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
    g++ -std=c++17 -Ilib/scorebot/src test/test_player_ui_rules.cpp -o "$test_dir/player_ui_rules"
    "$test_dir/player_ui_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_leaderboard_ui_rules.cpp -o "$test_dir/leaderboard_ui_rules"
    "$test_dir/leaderboard_ui_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_light_color_rules.cpp -o "$test_dir/light_color_rules"
    "$test_dir/light_color_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_replication_rules.cpp -o "$test_dir/replication_rules"
    "$test_dir/replication_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_board_identity.cpp -o "$test_dir/board_identity"
    "$test_dir/board_identity"
    g++ -std=c++17 -Ilib/scorebot/src test/test_button_input_rules.cpp -o "$test_dir/button_input_rules"
    "$test_dir/button_input_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_ota_transfer_rules.cpp -o "$test_dir/ota_transfer_rules"
    "$test_dir/ota_transfer_rules"
    g++ -std=c++17 -Ilib/scorebot/src test/test_protocol.cpp -o "$test_dir/protocol"
    "$test_dir/protocol"
    g++ -std=c++17 -Ilib/scorebot/src test/test_display_brightness.cpp -o "$test_dir/display_brightness"
    "$test_dir/display_brightness"
    g++ -std=c++17 -Ilib/scorebot/src test/test_visual_feedback_rules.cpp -o "$test_dir/visual_feedback_rules"
    "$test_dir/visual_feedback_rules"
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
