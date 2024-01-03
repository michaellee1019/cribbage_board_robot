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
    #define PIN_BUTTON_3  6
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
    pinMode(PIN_BUTTON_0, INPUT);
    pinMode(PIN_BUTTON_1, INPUT);
    pinMode(PIN_BUTTON_2, INPUT);
    pinMode(PIN_BUTTON_3, INPUT);

    digitalWrite(PIN_BUTTON_0, HIGH);
    digitalWrite(PIN_BUTTON_1, HIGH);
    digitalWrite(PIN_BUTTON_2, HIGH);
    digitalWrite(PIN_BUTTON_3, HIGH);

    pinMode(PIN_DIP_0, INPUT);
    pinMode(PIN_DIP_1, INPUT);
    pinMode(PIN_DIP_2, INPUT);
    pinMode(PIN_DIP_3, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);

    // pinMode(PIN_CLK, OUTPUT);
    // pinMode(PIN_CLK, OUTPUT);
    display.setBrightness(0x0f);

//    b.attach(PIN_BUTTON_0, INPUT);
//    b.interval(1);

    auto mode = digitalRead(PIN_DIP_0) << 0 |
                digitalRead(PIN_DIP_1) << 1 |
                digitalRead(PIN_DIP_2) << 2 |
                digitalRead(PIN_DIP_3) << 3;

    board = new Scoreboard();

    pinMode(PIN_TURN_LED, OUTPUT);

    for(int i=0; i<3; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }
}

byte prevFive = HIGH;
byte prevOne = HIGH;
byte prevNeg1 = HIGH;
byte prevOk = HIGH;
int score=0;

void updateDisplayScoreLoop(){
    // 5pt
    byte fiveState = digitalRead(PIN_BUTTON_0);
    if (fiveState == LOW && fiveState != prevFive) {
        score+=5;
    }
    prevFive = fiveState;

    // 1pt
    byte oneState = digitalRead(PIN_BUTTON_1);
    if (oneState == LOW && oneState != prevOne) {
        score++;
    }
    prevOne = oneState;

    // -1pt
    byte neg1State = digitalRead(PIN_BUTTON_2);
    if (neg1State == LOW && neg1State != prevNeg1) {
        score-=1;
    }
    prevNeg1 = neg1State;

    // commit
    if (digitalRead(PIN_BUTTON_3) == LOW)
    {
        for (byte i = 0 ; i<2;i++){
            delay(250);
            display.clear();
            delay(250);
            display.showNumberDec(score, false);
        }
        score = 0;
    }

    display.showNumberDec(score, false);
    delay(100);
}


void loop() {
//    b.update();
//    if (b.fell()) {
//        digitalWrite(LED_BUILTIN, HIGH);
//    }
//    if (b.rose()) {
//        digitalWrite(LED_BUILTIN, LOW);
//    }
    updateDisplayScoreLoop();
}
