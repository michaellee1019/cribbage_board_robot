#ifndef BOARDTYPES_HPP
#define BOARDTYPES_HPP

#include <RF24.h>

#include <Types.hpp>

static constexpr const byte playerAddresses_[MAX_PLAYERS][5] = {
    // TODO: make the last digit the board id / player number.
    {'R', 'x', 'A', 'A', 'A'},
    {'R', 'x', 'A', 'A', 'B'},
    {'R', 'x', 'A', 'A', 'C'},
//    {'R', 'x', 'A', 'A', 'D'},
};

inline static constexpr const byte* myBoardAddress() {
    static_assert(BOARD_ID < MAX_PLAYERS);
    if constexpr (BOARD_ID == -1 || BOARD_ID == 0) {
        return playerAddresses_[0];
    }
    return playerAddresses_[BOARD_ID];
}

inline static constexpr const byte* playerAddress(PlayerNumberT playerNumberT) {
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

    rf24_gpio_pin_t pinRadioCE;
    rf24_gpio_pin_t pinRadioCSN;
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
