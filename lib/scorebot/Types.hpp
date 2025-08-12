#ifndef TYPES_HPP
#define TYPES_HPP

#include <Arduino.h>
#include <optional>

class MeshAddress {
    byte _value[5];
public:
    explicit constexpr MeshAddress(byte b0, byte b1, byte b2, byte b3, byte b4)
    : _value{b0, b1, b2, b3, b4} {}

    [[nodiscard]] constexpr const byte* value() const {
        return _value;
    }
    
    friend std::ostream& operator<<(std::ostream& out, const MeshAddress& self);
};

using PlayerNumberT = int;
using ScoreT = int;
using TurnNumberT = int;
using TimestampT = decltype(millis());

template <typename T>
void print(const T& t) {
    Serial.print(t);
}

template <typename T>
void println(const T& t) {
    Serial.println(t);
}

inline void println() {
    Serial.println();
}

// TODO: Make this configurable.
static constexpr int MAX_DISPLAYS = 3;
static constexpr int MAX_PLAYERS = MAX_DISPLAYS;

enum class BoardRole {
    Leader,
    Player_Red,
    Player_Blue,
    Player_Green,
    Player_White,
};

class BoardRoleConfig {
public:
    BoardRole role;
    std::string name;
    int playerNumber;
    MeshAddress meshAddress;
    
    BoardRoleConfig(BoardRole r, const std::string& n, int num, const MeshAddress& addr) 
        : role(r), name(n), playerNumber(num), meshAddress(addr) {}
};

#endif