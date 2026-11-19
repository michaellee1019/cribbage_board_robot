#pragma once
#include <PairingRules.hpp>
namespace scorebot {
constexpr bool mayAcceptLeaderMessage(bool localAuthority, uint32_t selectedLeader,
                                     uint32_t transportSender, uint32_t claimedLeader) {
    return !localAuthority && selectedLeader != 0 && transportSender == selectedLeader && claimedLeader == selectedLeader;
}
}
