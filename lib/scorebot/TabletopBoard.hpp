#ifndef TABLETOPBOARD_HPP
#define TABLETOPBOARD_HPP

#include <RF24.h>

struct IOConfig {
    int pinCommit;
    int pinNegOne;
    int pinPlusFive;
    int pinPlusOne;
    int pinPassTurn;

    int pinLedBuiltin;
    int pinTurnLed;

    rf24_gpio_pin_t pinRadioCE;
    rf24_gpio_pin_t pinRadioCSN;
};

class TabletopBoard {
public:
    virtual ~TabletopBoard() = default;
    virtual void setup() = 0;
    virtual void loop() = 0;
    explicit TabletopBoard();
};

void blink();


#endif  // TABLETOPBOARD_HPP
