#include <Arduino.h>
#include <TM1637Display.h>
#include <Bounce2.h>


#if 0
Random Notes
============

    Board = Master <-> Sender
    P1 = Slave <-> Receiver RxAAA
    P2 = Slave <-> Receiver RxBBB
    P3 = Slave <-> Receiver RxCCC
    P4 = Slave <-> Receiver RxDDD

#endif

// <RF Module>
    // Chip Enable
    #define PIN_CE      9
    // Chip Select Not
    #define PIN_CSN     10
// </RF Module>

// <TM1637>
    #define PIN_CLK   8
    #define PIN_DIO   7
// </TM1637>

// <Turn LED>
    #define PIN_TURN_LED  2
// </Turn LED>

// <Buttons>
    // Plus 5
    #define PIN_BUTTON_0  3
    // Plus 1
    #define PIN_BUTTON_1  4
    // Minus 1
    #define PIN_BUTTON_2  5
    // Commit
    #define PIN_BUTTON_1  6
// </Buttons>


// <DIP Switches>
    #define PIN_DIP_0   14
    #define PIN_DIP_1   15
    #define PIN_DIP_2   16
    #define PIN_DIP_3   17
// </DIP Switches>

class Board {
public:
    virtual void setup() = 0;
    virtual ~Board() = default;
};
class Scoreboard : public Board {
public:
    void setup() override {
    }
    ~Scoreboard() override = default;
};

class Blinkboard : public Board {
public:
    void setup() override {
    }
    ~Blinkboard() override = default;
};

static Board* board;
static TM1637Display display(PIN_CLK, PIN_DIO);

static Bounce b = Bounce(); // Instantiate a Bounce object

void setup() {
    pinMode(PIN_DIP_0, INPUT);
    pinMode(PIN_DIP_1, INPUT);
    pinMode(PIN_DIP_2, INPUT);
    pinMode(PIN_DIP_3, INPUT);

    // pinMode(PIN_CLK, OUTPUT);
    // pinMode(PIN_CLK, OUTPUT);
    display.setBrightness(0x0f);

    b.attach(PIN_BUTTON_0, INPUT);
    b.interval(1);

    auto mode = digitalRead(PIN_DIP_0) << 0 |
                digitalRead(PIN_DIP_1) << 1 |
                digitalRead(PIN_DIP_2) << 2 |
                digitalRead(PIN_DIP_3) << 3;

    board = new Scoreboard();

    pinMode(PIN_TURN_LED, OUTPUT);
}



void loop() {
    b.update();
    if (b.fell()) {
        digitalWrite(PIN_TURN_LED, HIGH);
    }
    if (b.rose()) {
        digitalWrite(PIN_TURN_LED, LOW);
    }
}
