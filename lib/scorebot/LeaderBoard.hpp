#ifndef LEADERBOARD_HPP
#define LEADERBOARD_HPP

#include "Devices.hpp"

class LeaderBoard : public TabletopBoard {
public:
    explicit LeaderBoard(IOConfig config);
    ~LeaderBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};


#endif  // LEADERBOARD_HPP
