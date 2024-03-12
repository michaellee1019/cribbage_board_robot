#ifndef LEADERBOARD_HPP
#define LEADERBOARD_HPP

#include "TabletopBoard.hpp"

class LeaderBoard final : public TabletopBoard {
public:
    explicit LeaderBoard(const IOConfig& config);
    ~LeaderBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};


#endif  // LEADERBOARD_HPP
