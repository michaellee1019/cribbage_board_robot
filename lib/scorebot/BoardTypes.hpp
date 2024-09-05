#ifndef BOARDTYPES_HPP
#define BOARDTYPES_HPP

#include <Types.hpp>
#include <iosfwd>

// Idk if this is actually valuable.
class PlayerAddress {
    byte _value[5];
public:
    explicit constexpr PlayerAddress(byte b0, byte b1, byte b2, byte b3, byte b4)
    : _value{b0, b1, b2, b3, b4} {}

    [[nodiscard]] constexpr const byte* value() const {
        return _value;
    }
    friend std::ostream&
    operator<<(std::ostream& out, const PlayerAddress& self);
};

inline std::ostream&
operator<<(std::ostream& out, const PlayerAddress& self) {
    out << "@";
    for (unsigned char i : self._value) {
        out << i;
    }
    return out;
}

static constexpr PlayerAddress playerAddresses_[MAX_PLAYERS] = {
    // TODO: make the last digit the board id / player number.
        PlayerAddress{'R', 'x', 'A', 'A', 'A'},
        PlayerAddress{'R', 'x', 'A', 'A', 'B'},
        PlayerAddress{'R', 'x', 'A', 'A', 'C'},
//        PlayerAddress{'R', 'x', 'A', 'A', 'D'},
};

inline static constexpr PlayerAddress myBoardAddress() {
    static_assert(BOARD_ID < MAX_PLAYERS);
    return BOARD_ID == -1 ? playerAddresses_[0] :  playerAddresses_[BOARD_ID];
}



inline static constexpr PlayerAddress playerAddress(PlayerNumberT playerNumberT) {
    return playerAddresses_[playerNumberT];
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
