#pragma once

#include <BoardIdentity.hpp>
#include <BoardRole.hpp>

namespace scorebot {

// A passive display is an unknown board whose identity characteristic is the
// id derived from its own public Bluetooth address. It cannot impersonate a
// configured player or the leaderboard, and it is not part of the roster.
constexpr bool isDisplaySubscriber(
    uint32_t identity, uint64_t bluetoothMac, BoardRole configured) {
    if (identity == 0 || configured != BoardRole::Unknown) {
        return false;
    }
    // DevKit and XIAO firmware disagree on whether Bluetooth is base+1 or
    // base+2. Accept either so the wall bridge does not depend on that
    // compile flag matching the leaderboard's.
    return identity == boardIdFromBluetoothMac(bluetoothMac, 1) ||
           identity == boardIdFromBluetoothMac(bluetoothMac, 2);
}

}  // namespace scorebot
