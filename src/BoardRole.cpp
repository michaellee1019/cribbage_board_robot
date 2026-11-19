#include <BoardRole.hpp>
#include <LeaderIdentity.hpp>

namespace {
constexpr BoardRoleConfig kUnknownRole{};
constexpr BoardRoleConfig kBoardRoles[] = {
    {BoardRole::Player_Red, 2934574912, 0x983daeea0f40ULL, "RED", 1, 0xFF0000},
    {BoardRole::Player_Blue, 860931256, 0x64e83350c4b8ULL, "BLUE", 2, 0x0000FF},
    {BoardRole::Player_Green, 2934574676, 0x983daeea0e54ULL, "GREN", 3, 0x00FF00},
    {BoardRole::Player_White, 2934416992, 0x983daee7a660ULL, "WHIT", 4, 0xFFFFFF},
};
constexpr BoardRoleConfig kLeaderRole{
    BoardRole::Leader, scorebot::kLeaderNodeId,
    scorebot::kExternalLeader ? 0 : 0x983daeea17bcULL, "LEADER", 0, 0x000000};

constexpr bool externalIdentityIsDistinct() {
    if (!scorebot::kExternalLeader) {
        return true;
    }
    // Include the retired hardware leader so two devices cannot acquire the
    // same role when running this configuration.
    if (scorebot::kLeaderNodeId == 2934577084u ||
        scorebot::kLeaderBluetoothMac == 0x983daeea17bdULL ||
        scorebot::kLeaderBluetoothMac == 0x983daeea17beULL) {
        return false;
    }
    for (const auto& player : kBoardRoles) {
        if (player.nodeId == scorebot::kLeaderNodeId ||
            player.baseMac + 1 == scorebot::kLeaderBluetoothMac ||
            player.baseMac + 2 == scorebot::kLeaderBluetoothMac) {
            return false;
        }
    }
    return true;
}
static_assert(externalIdentityIsDistinct(), "Pi identity must not collide with a hardware board");
}  // namespace

const BoardRoleConfig& getRoleConfig(uint32_t nodeId) {
    if (nodeId == kLeaderRole.nodeId) {
        return kLeaderRole;
    }
    for (const BoardRoleConfig& config : kBoardRoles) {
        if (config.nodeId == nodeId) {
            return config;
        }
    }
    return kUnknownRole;
}

const BoardRoleConfig& getRoleConfigForBluetoothMac(uint64_t bluetoothMac, uint8_t offset) {
    if (bluetoothMac == (scorebot::kExternalLeader
                             ? scorebot::kLeaderBluetoothMac
                             : kLeaderRole.baseMac + offset)) {
        return kLeaderRole;
    }
    for (const BoardRoleConfig& config : kBoardRoles) {
        if (config.baseMac + offset == bluetoothMac) {
            return config;
        }
    }
    return kUnknownRole;
}

uint32_t getNodeIdForRole(BoardRole role) {
    if (role == BoardRole::Leader) {
        return kLeaderRole.nodeId;
    }
    for (const BoardRoleConfig& config : kBoardRoles) {
        if (config.role == role) {
            return config.nodeId;
        }
    }
    return 0;
}
