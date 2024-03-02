#ifndef TABLETOPBOARD_HPP
#define TABLETOPBOARD_HPP

struct IOConfig {
    int pinButton0;
    int pinButton1;
    int pinButton2;
    int pinButton3;
    int pinButton4;

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


#endif  // TABLETOPBOARD_HPP
