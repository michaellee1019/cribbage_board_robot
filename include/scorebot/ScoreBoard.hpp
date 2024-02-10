#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP

#include <scorebot/Devices.hpp>

class ScoreBoard : public TabletopBoard {
public:
    explicit ScoreBoard(IOConfig config);
    ~ScoreBoard() override;
    void setup(const IOConfig& config) override;
    void loop() override;
    struct Impl;
    Impl* impl;
};


#endif // SCOREBOARD_HPP

