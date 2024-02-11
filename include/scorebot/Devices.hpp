#ifndef DEVICES_HPP
#define DEVICES_HPP

#include <ArduinoSTL.h>

struct IOConfig {
    int pinButton0;
    int pinButton1;
    int pinButton2;
    int pinButton3;

    int pinDip0;
    int pinDip1;
    int pinDip2;
    int pinDip3;

    int pinLedBuiltin;
    int pinTurnLed;
};

class TabletopBoard {
public:
    virtual ~TabletopBoard() = default;
    virtual void setup() = 0;
    virtual void loop() = 0;
    explicit TabletopBoard();
};

void blink();


#endif  // DEVICES_HPP
