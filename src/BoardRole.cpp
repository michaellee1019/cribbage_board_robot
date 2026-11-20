#include <BoardRole.hpp>

namespace {
constexpr BoardRoleConfig kStyles[] = {
    {BoardRole::Unknown, "COLR", -1, 0},
    {BoardRole::Leader, "HOST", 0, 0},
    {BoardRole::Player_Red, "RED ", 1, 0xff0000},
    {BoardRole::Player_Blue, "BLUE", 2, 0x0000ff},
    {BoardRole::Player_Green, "GREN", 3, 0x00ff00},
    {BoardRole::Player_Yellow, "YELL", 4, 0xffff00},
};
}

const BoardRoleConfig& getRoleStyle(BoardRole role) {
    const auto index = static_cast<unsigned>(role);
    return kStyles[index < 6 ? index : 0];
}

BoardRole suggestedRole(uint32_t nodeId) {
    // Enclosure defaults only. Never used for admission or hardware detection.
    switch (nodeId) {
        case 2934574912u: return BoardRole::Player_Red;
        case 860931256u: return BoardRole::Player_Blue;
        case 2934574676u: return BoardRole::Player_Green;
        case 2934416992u: return BoardRole::Player_Yellow;
        default: return BoardRole::Unknown;
    }
}
