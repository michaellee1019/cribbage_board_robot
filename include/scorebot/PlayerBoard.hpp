#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <scorebot/Devices.hpp>

class PlayerBoard : public IODevice {
public:
    ~PlayerBoard() override;
    void setup() override;
    void loop() override;
};


#endif // PLAYERBOARD_HPP
