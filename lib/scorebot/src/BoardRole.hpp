#ifndef BOARDROLE_H
#define BOARDROLE_H

#include <cstdint>
#include <string>
#include <optional>
#include <map>

enum class BoardRole {
    Unknown,
    Leader,
    Player_Red,
    Player_Blue,
    Player_Green,
    Player_White,
};

class BoardRoleConfig {
public:
    BoardRole role;
    uint32_t nodeId;
    std::string name;
    int playerNumber;
    
    // Default constructor
    BoardRoleConfig() : role(BoardRole::Unknown), nodeId(0), name("UNKNOWN"), playerNumber(-1) {}
    
    // Parameterized constructor
    BoardRoleConfig(BoardRole r, uint32_t nodeId, const std::string& n, int num) 
        : role(r), nodeId(nodeId), name(n), playerNumber(num) {}
};

// Function declarations
BoardRoleConfig getRoleConfig(uint32_t nodeId);
uint32_t getNodeIdForRole(BoardRole role);

// External variable declaration
extern std::map<uint32_t, BoardRoleConfig> boardRoleConfig;

#endif // BOARDROLE_H