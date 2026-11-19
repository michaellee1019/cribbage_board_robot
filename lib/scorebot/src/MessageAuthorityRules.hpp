#pragma once

#include <BoardRole.hpp>
#include <Protocol.hpp>

namespace scorebot {

constexpr bool mayAcceptSnapshot(BoardRole localRole, BoardRole transportSenderRole) {
    const bool localIsPlayer = localRole == BoardRole::Player_Red ||
                               localRole == BoardRole::Player_Blue ||
                               localRole == BoardRole::Player_Green ||
                               localRole == BoardRole::Player_White;
    return localIsPlayer && transportSenderRole == BoardRole::Leader;
}

inline uint32_t trustedLeaderForBluetoothAddress(
    bool publicAddress, uint64_t bluetoothMac, uint8_t offset) {
    if (!publicAddress) {
        return 0;
    }
    const auto& config = getRoleConfigForBluetoothMac(bluetoothMac, offset);
    return config.role == BoardRole::Leader ? config.nodeId : 0;
}

inline bool isConfiguredLeaderPeer(
    uint32_t nodeId, uint16_t protocol, bool publicAddress,
    uint64_t bluetoothMac, uint8_t offset) {
    return protocol == kWireProtocolVersion && nodeId != 0 &&
           nodeId == trustedLeaderForBluetoothAddress(publicAddress, bluetoothMac, offset);
}

inline bool mayAcceptLeaderMessage(
    BoardRole localRole, uint32_t transportSender, uint32_t claimedLeader) {
    return transportSender == claimedLeader &&
           transportSender == getNodeIdForRole(BoardRole::Leader) &&
           mayAcceptSnapshot(localRole, getRoleConfig(transportSender).role);
}

}  // namespace scorebot
