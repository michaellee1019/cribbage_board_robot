#ifndef BOARDROLE_H
#define BOARDROLE_H

#include <cstdint>

enum class BoardRole {
    Unknown,
    Leader,
    Player_Red,
    Player_Blue,
    Player_Green,
    Player_Yellow,
    Player_White = Player_Yellow,  // Existing saved color remains role 5.
};

struct BoardRoleConfig {
    BoardRole role{BoardRole::Unknown};
    const char* name{"UNKNOWN"};
    int8_t playerNumber{-1};
    uint32_t color{0};
};

const BoardRoleConfig& getRoleStyle(BoardRole role);
BoardRole suggestedRole(uint32_t nodeId);

#endif // BOARDROLE_H
