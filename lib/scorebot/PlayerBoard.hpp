#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <Message.hpp>

#include "TabletopBoard.hpp"

const byte slaveAddresses[N_PLAYERS][5] = {
    {'R', 'x', 'A', 'A', 'A'},
    {'R', 'x', 'A', 'A', 'B'},
    {'R', 'x', 'A', 'A', 'C'},
};

inline const byte* myBoardAddress() {
    static_assert(BOARD_ID < N_PLAYERS);
    if constexpr (BOARD_ID == -1 || BOARD_ID == 0) {
        return slaveAddresses[0];
    }
    return slaveAddresses[BOARD_ID];
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
