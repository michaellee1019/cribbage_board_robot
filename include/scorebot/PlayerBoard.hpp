#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <scorebot/Devices.hpp>

class PlayerBoard : public TabletopBoard {
public:
    explicit PlayerBoard();
    ~PlayerBoard() override;
    void setup(const IOConfig& config) override;
    void loop() override;
    struct Impl;
    Impl* impl;
};


#endif // PLAYERBOARD_HPP
