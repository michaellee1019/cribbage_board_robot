#include <ButtonInputRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::ButtonAction;

int main() {
    assert(scorebot::buttonCapturedPressed(0, 0b11110));
    assert(scorebot::buttonCapturedPressed(4, 0b01111));
    assert(!scorebot::buttonCapturedPressed(0, 0b11111));
    assert(!scorebot::buttonCapturedPressed(4, 0b11111));
    assert(!scorebot::buttonCapturedPressed(16, 0));
    assert(scorebot::buttonActionForPin(false, 0) == ButtonAction::Add);
    assert(scorebot::buttonActionForPin(false, 1) == ButtonAction::NegativeOne);
    assert(scorebot::buttonActionForPin(false, 2) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 3) == ButtonAction::PlusFive);
    assert(scorebot::buttonActionForPin(false, 4) == ButtonAction::Ok);
    assert(scorebot::buttonActionForPin(true, 0) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(true, 1) == ButtonAction::Add);
    assert(scorebot::buttonActionForPin(true, 2) == ButtonAction::Ok);
    assert(scorebot::buttonActionForPin(true, 3) == ButtonAction::NegativeOne);
    assert(scorebot::buttonActionForPin(true, 4) == ButtonAction::None);
    // Physical Blue +1/+5 use the alternate MCP wiring regardless of color.
    assert(scorebot::buttonActionForPin(false, 3, 860931256u) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 2, 860931256u) == ButtonAction::PlusFive);
    assert(scorebot::buttonActionForPin(false, 2, 2934574912u) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 2, 2934416992u) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(true, 2, 860931256u) == ButtonAction::Ok);
    assert(scorebot::usesPlayerBoardV9(0x76c3cfc0));
    assert(!scorebot::usesPlayerBoardV9(860931256u));
    assert(!scorebot::usesPlayerBoardV9(2934574912u));
    assert(scorebot::buttonActionForPin(false, 3, 0x76c3cfc0) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 2, 0x76c3cfc0) == ButtonAction::PlusFive);
    std::cout << "Button-input tests passed\n";
    return 0;
}
