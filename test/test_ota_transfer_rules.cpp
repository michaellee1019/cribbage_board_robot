#if !defined(ARDUINO)

#include <OtaTransferRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    assert(scorebot::otaTransferOwnedBy(true, 7, 7));
    assert(!scorebot::otaTransferOwnedBy(true, 7, 8));
    assert(!scorebot::otaTransferOwnedBy(false, 7, 7));
    assert(scorebot::otaCanAppend(true, 7, 7, 40, 100, 60));
    assert(!scorebot::otaCanAppend(true, 7, 8, 40, 100, 1));
    assert(!scorebot::otaCanAppend(true, 7, 7, 40, 100, 61));
    assert(!scorebot::otaCanAppend(true, 7, 7, 40, 100, 0));
    assert(!scorebot::otaCanAppend(true, 7, 7, 101, 100, 1));
    assert(scorebot::otaTransferTimedOut(true, 31, 0, 30));
    assert(!scorebot::otaTransferTimedOut(true, 30, 0, 30));
    assert(!scorebot::otaTransferTimedOut(false, 99, 0, 30));
    std::cout << "OTA-transfer-rule tests passed\n";
}

#endif
