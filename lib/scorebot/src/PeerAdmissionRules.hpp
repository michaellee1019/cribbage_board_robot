#pragma once

#include <BoardRole.hpp>
#include <DisplayPeerRules.hpp>
#include <Protocol.hpp>

namespace scorebot {

constexpr bool isPlayerRole(BoardRole role) {
    return role == BoardRole::Player_Red || role == BoardRole::Player_Blue ||
           role == BoardRole::Player_Green || role == BoardRole::Player_White;
}

constexpr uint8_t roleMask(BoardRole role) {
    return isPlayerRole(role)
               ? static_cast<uint8_t>(1u << (static_cast<uint8_t>(role) -
                                             static_cast<uint8_t>(BoardRole::Player_Red)))
               : 0;
}

enum class PeerKind { Rejected, Player, Display };

struct PeerAdmission {
    PeerKind kind{PeerKind::Rejected};
    uint8_t playerMask{0};

    // The caller checks whether the roster is frozen, including again under
    // the insertion lock. Displays never occupy a player roster slot.
    constexpr bool eligibleForFrozenRoster(uint8_t rosterMask) const {
        return kind == PeerKind::Display ||
               (kind == PeerKind::Player && (rosterMask & playerMask) != 0);
    }
};

inline PeerAdmission classifyPeerAdmission(
    uint32_t nodeId, uint16_t remoteProtocol, bool publicAddress,
    uint64_t bluetoothMac, uint8_t bluetoothMacOffset, uint32_t localPeerId) {
    if (remoteProtocol != kWireProtocolVersion || !publicAddress ||
        nodeId == 0 || nodeId == localPeerId) {
        return {};
    }

    const BoardRole role = getRoleConfig(nodeId).role;
    if (isDisplaySubscriber(nodeId, bluetoothMac, role)) {
        return {PeerKind::Display, 0};
    }
    if (isPlayerRole(role) &&
        nodeId == getRoleConfigForBluetoothMac(bluetoothMac, bluetoothMacOffset).nodeId) {
        return {PeerKind::Player, roleMask(role)};
    }
    return {};
}

}  // namespace scorebot
