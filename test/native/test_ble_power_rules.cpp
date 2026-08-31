#include <BlePowerRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using namespace scorebot::ble_power;

    auto result = recordUnexpectedDisconnect({}, 100);
    assert(!result.increasePower);
    assert(result.window.count == 1);
    result = recordUnexpectedDisconnect(result.window, 200);
    assert(!result.increasePower);
    assert(result.window.count == 2);
    result = recordUnexpectedDisconnect(result.window, 300);
    assert(result.increasePower);
    assert(result.window.count == 0);

    result = recordUnexpectedDisconnect(
        {100, 2}, 100 + kDisconnectWindowMs + 1);
    assert(!result.increasePower);
    assert(result.window.count == 1);

    assert(nextConnectionPowerDbm(kInitialConnectionPowerDbm) ==
           kIntermediateConnectionPowerDbm);
    assert(nextConnectionPowerDbm(kIntermediateConnectionPowerDbm) ==
           kMaximumConnectionPowerDbm);
    assert(nextConnectionPowerDbm(kMaximumConnectionPowerDbm) ==
           kMaximumConnectionPowerDbm);

    std::cout << "BLE power-rule tests passed\n";
}
