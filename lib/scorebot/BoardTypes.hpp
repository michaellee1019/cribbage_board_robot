#ifndef BOARDTYPES_HPP
#define BOARDTYPES_HPP

#include <Types.hpp>
#include <iosfwd>
#include <cstring>
#include <map>

// Stream operator for MeshAddress (moved from Types.hpp)
inline std::ostream&
operator<<(std::ostream& out, const MeshAddress& self) {
    out << "@";
    for (unsigned char i : self._value) {
        out << i;
    }
    return out;
}

std::map<BoardRole, BoardRoleConfig> boardRoleConfig = {
    {BoardRole::Player_Red, {BoardRole::Player_Red, "RED", 1, MeshAddress{'R', 'x', 'A', 'A', 'R'}}},
    {BoardRole::Player_Blue, {BoardRole::Player_Blue, "BLUE", 2, MeshAddress{'R', 'x', 'A', 'A', 'B'}}},
    {BoardRole::Player_Green, {BoardRole::Player_Green, "GREN", 3, MeshAddress{'R', 'x', 'A', 'A', 'G'}}},
    {BoardRole::Player_White, {BoardRole::Player_White, "WHIT", 4, MeshAddress{'R', 'x', 'A', 'A', 'W'}}},
    {BoardRole::Leader, {BoardRole::Leader, "LEADER", 0, MeshAddress{'R', 'x', 'A', 'A', 'L'}}},
};

inline std::optional<BoardRoleConfig> getMyRoleConfig(BoardRole role) {
    auto it = boardRoleConfig.find(role);
    return it != boardRoleConfig.end() ? std::make_optional(it->second) : std::nullopt;
}

inline BoardRole myBoardRole(uint8_t* macAddress) {
    uint8_t leader[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //red 98:3d:ae:ea:0f:40
    uint8_t red[] = {0x98, 0x3d, 0xae, 0xea, 0x0f, 0x40};

    //blue 64:e8:33:50:c4:b8
    uint8_t blue[] = {0x64, 0xe8, 0x33, 0x50, 0xc4, 0xb8};

    //green 98:3d:ae:ea:0e:54
    uint8_t green[] = {0x98, 0x3d, 0xae, 0xea, 0x0e, 0x54};

    //white 98:3d:ae:e7:a6:60
    uint8_t white[] = {0x98, 0x3d, 0xae, 0xe7, 0xa6, 0x60};


    if (memcmp(macAddress, leader, 6) == 0) return BoardRole::Leader;
    if (memcmp(macAddress, red, 6) == 0) return BoardRole::Player_Red;
    if (memcmp(macAddress, blue, 6) == 0) return BoardRole::Player_Blue;
    if (memcmp(macAddress, green, 6) == 0) return BoardRole::Player_Green;
    if (memcmp(macAddress, white, 6) == 0) return BoardRole::Player_White;
    
    // leader is default with address 98:3d:ae:ea:17:bc
    return BoardRole::Leader; // default
}

struct IOConfig {
    int pinCommit;
    int pinNegOne;
    int pinPlusFive;
    int pinPlusOne;
    int pinPassTurn;

    int pinLedBuiltin;
    int pinTurnLed;
};

class TabletopBoard {
public:
    virtual ~TabletopBoard() = default;
    virtual void setup() = 0;
    virtual void loop() = 0;
    explicit TabletopBoard();
};

class PlayerBoard final : public TabletopBoard {
public:
    explicit PlayerBoard(const IOConfig& config, TimestampT startupGeneration);
    ~PlayerBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};

class LeaderBoard final : public TabletopBoard {
public:
    explicit LeaderBoard(const IOConfig& config, TimestampT startupGeneration);
    ~LeaderBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};

#endif  // BOARDTYPES_HPP
