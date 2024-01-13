#ifndef DEVICES_HPP
#define DEVICES_HPP

#include <ArduinoSTL.h>

class IODevice {
public:
    virtual ~IODevice() = default;
    virtual void setup() = 0;
    virtual void loop() = 0;
    explicit IODevice();
};

struct IOConfig {
    int pinButton0;
    int pinButton1;
    int pinButton2;
    int pinButton3;
};

void scorebotSetup(IOConfig config);
void scorebotLoop();

#endif //DEVICES_HPP
