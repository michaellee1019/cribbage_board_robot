#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <Message.hpp>

#include "TabletopBoard.hpp"

static constexpr const byte playerAddresses_[MAX_PLAYERS][5] = {
    // TODO: make the last digit the board id / player number.
    {'R', 'x', 'A', 'A', 'A'},
    {'R', 'x', 'A', 'A', 'B'},
    {'R', 'x', 'A', 'A', 'C'},
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
