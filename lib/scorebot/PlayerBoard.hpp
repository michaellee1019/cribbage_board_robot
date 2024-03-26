#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <Message.hpp>

#include "TabletopBoard.hpp"

const byte _playerAddresses[MAX_PLAYERS][5] = {
    // TODO: make the last digit the board id / player number.
    {'R', 'x', 'A', 'A', 'A'},
    {'R', 'x', 'A', 'A', 'B'},
    {'R', 'x', 'A', 'A', 'C'},
};

inline const byte* myBoardAddress() {
    static_assert(BOARD_ID < MAX_PLAYERS);
    if constexpr (BOARD_ID == -1 || BOARD_ID == 0) {
        return _playerAddresses[0];
    }
    return _playerAddresses[BOARD_ID];
}

inline const byte* playerAddress(PlayerNumberT playerNumberT) {
    return _playerAddresses[playerNumberT];
}


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


#endif  // PLAYERBOARD_HPP
