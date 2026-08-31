#include <LightColorRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    assert(scorebot::activePlayerColor(0xff0000) == 0x200000);
    assert(scorebot::activePlayerColor(0x00ff00) == 0x002000);
    assert(scorebot::activePlayerColor(0x0000ff) == 0x000020);
    assert(scorebot::activePlayerColor(0xffffff) == 0x202020);
    assert(scorebot::activePlayerColor(0) == 0);
    std::cout << "Light-color tests passed\n";
}
