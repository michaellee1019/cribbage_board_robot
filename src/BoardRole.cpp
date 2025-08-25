#include <cstdint>
#include <string>
#include <map>
#include <cstddef>
#include <Arduino.h>
#include <BoardRole.hpp>

std::map<uint32_t, BoardRoleConfig> boardRoleConfig = {
    {2934574912, {BoardRole::Player_Red, 2934574912, "RED", 1}},
    {860931256, {BoardRole::Player_Blue, 860931256, "BLUE", 2}},
    {2934574676, {BoardRole::Player_Green, 2934574676, "GREN", 3}},
    {2934416992, {BoardRole::Player_White, 2934416992, "WHIT", 4}},
    {2934577084, {BoardRole::Leader, 2934577084, "LEADER", 0}},
    {0, {BoardRole::Unknown, 0, "UNKNOWN", -1}},
};

BoardRoleConfig getRoleConfig(uint32_t nodeId) {
    auto it = boardRoleConfig.find(nodeId);
    return it != boardRoleConfig.end() ? it->second : boardRoleConfig[0];
}

uint32_t getNodeIdForRole(BoardRole role) {
    // Find nodeId for a given role in our connected nodes
    for (const auto& pair : boardRoleConfig) {
        if (pair.second.role == role) {
            return pair.first;
        }
    }
    return 0; // Role not found in connected nodes
}