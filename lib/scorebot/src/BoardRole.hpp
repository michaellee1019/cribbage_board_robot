#ifndef BOARDROLE_H
#define BOARDROLE_H

#include <cstdint>

enum class BoardRole {
    Unknown,
    Leader,
    Player_Red,
    Player_Blue,
    Player_Green,
    Player_White,
};

struct BoardRoleConfig {
    BoardRole role{BoardRole::Unknown};
    uint32_t nodeId{0};
    uint64_t baseMac{0};
    const char* name{"UNKNOWN"};
    int8_t playerNumber{-1};
    uint32_t color{0};
};

const BoardRoleConfig& getRoleConfig(uint32_t nodeId);
const BoardRoleConfig& getRoleConfigForBluetoothMac(uint64_t bluetoothMac, uint8_t offset);
uint32_t getNodeIdForRole(BoardRole role);

#endif // BOARDROLE_H
