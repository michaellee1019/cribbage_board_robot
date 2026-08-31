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
    assert(scorebot::buttonActionForPin(false, 2) == ButtonAction::PlusFive);
    assert(scorebot::buttonActionForPin(false, 3) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 4) == ButtonAction::Ok);
    assert(scorebot::buttonActionForPin(true, 0) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(true, 1) == ButtonAction::Add);
    assert(scorebot::buttonActionForPin(true, 2) == ButtonAction::Ok);
    assert(scorebot::buttonActionForPin(true, 3) == ButtonAction::NegativeOne);
    assert(scorebot::buttonActionForPin(true, 4) == ButtonAction::None);
    std::cout << "Button-input tests passed\n";
    return 0;
}
