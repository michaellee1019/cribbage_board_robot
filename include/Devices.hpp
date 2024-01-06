#ifndef CRIBBAGE_BOARD_ROBOT_DEVICES_HPP
#define CRIBBAGE_BOARD_ROBOT_DEVICES_HPP

class IODevice {
    virtual ~IODevice() = default;
    virtual void loop() = 0;
};


#endif //CRIBBAGE_BOARD_ROBOT_DEVICES_HPP
