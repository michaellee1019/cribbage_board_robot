#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP

#include <scorebot/Devices.hpp>

class ScoreBoard : public IODevice {
public:
    ~ScoreBoard() override;
    void setup(const IOConfig& config) override;
    void loop() override;
};


#endif // SCOREBOARD_HPP

