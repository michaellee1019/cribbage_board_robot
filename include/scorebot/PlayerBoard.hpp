#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <scorebot/Devices.hpp>

class PlayerBoard : public TabletopBoard {
public:
    explicit PlayerBoard(IOConfig config);
    ~PlayerBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};


#endif  // PLAYERBOARD_HPP
