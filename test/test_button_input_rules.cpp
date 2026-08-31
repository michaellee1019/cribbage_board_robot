#include <ButtonInputRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    assert(scorebot::buttonCapturedPressed(0, 0b11110));
    assert(scorebot::buttonCapturedPressed(4, 0b01111));
    assert(!scorebot::buttonCapturedPressed(0, 0b11111));
    assert(!scorebot::buttonCapturedPressed(4, 0b11111));
    assert(!scorebot::buttonCapturedPressed(16, 0));
    std::cout << "Button-input tests passed\n";
    return 0;
}
