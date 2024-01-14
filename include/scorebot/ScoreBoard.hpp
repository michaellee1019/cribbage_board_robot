#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP

#include <scorebot/Devices.hpp>

class ScoreBoard : public TabletopBoard {
public:
    ~ScoreBoard() override;
    void setup(const IOConfig& config) override;
    void loop() override;
};


#endif // SCOREBOARD_HPP

