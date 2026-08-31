#if !defined(ARDUINO)

#include <UsbConnectionRules.hpp>

#include <cassert>
#include <iostream>

int main() {
    using namespace scorebot::usb;

    static_assert(kDisconnectMissCount == 3);
    static_assert(kFinalSleepProbeMs == 25);

    ConnectionState state;
    assert(!state.connected());
    assert(!state.observe(false));

    assert(state.observe(true));
    assert(state.connected());
    assert(state.consecutiveMisses() == 0);

    assert(state.observe(false));
    assert(state.consecutiveMisses() == 1);
    assert(state.observe(false));
    assert(state.consecutiveMisses() == 2);

    // Any valid frame restores a fully connected state.
    assert(state.observe(true));
    assert(state.consecutiveMisses() == 0);

    assert(state.observe(false));
    assert(state.observe(false));
    assert(!state.observe(false));
    assert(!state.connected());
    assert(state.consecutiveMisses() == 0);

    // Reconnection is immediate after a confirmed disconnect.
    assert(state.observe(true));
    assert(state.connected());

    std::cout << "USB-connection rule tests passed\n";
}

#endif
