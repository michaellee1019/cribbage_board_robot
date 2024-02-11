#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP

#include <scorebot/Devices.hpp>

class LeaderBoard : public TabletopBoard {
public:
    explicit LeaderBoard(IOConfig config);
    ~LeaderBoard() override;
    void setup(const IOConfig& config) override;
    void loop() override;
    struct Impl;
    Impl* impl;
};


#endif  // SCOREBOARD_HPP
