#include <Arduino.h>
#include <TM1637Display.h>
#include "RF24.h"


//================


#define IS_SCORE_BOARD (BOARD_ID == -1)
// -1: Scoreboard
// 0: Player 1
// 1: Player 2
/*
 *     auto mode = digitalRead(PIN_DIP_0) << 0 |
                digitalRead(PIN_DIP_1) << 1 |
                digitalRead(PIN_DIP_2) << 2 |
                digitalRead(PIN_DIP_3) << 3;
 */


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

const byte numSlaves = 1;
const byte slaveAddresses[numSlaves][5] = {
        // each slave needs a different address
        {'R','x','A','A','A'},
        //{'R','x','A','A','B'}
};

RF24 radio(PIN_CE, PIN_CSN);

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

static TM1637Display display(PIN_CLK, PIN_DIO);

void setup() {
    Serial.begin(9600);

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

    pinMode(PIN_TURN_LED, OUTPUT);

    for(int i=0; i<3; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }

    if (BOARD_ID == -1) {
        // Scoreboard
        // RF Sender Setup
        radio.begin();
        radio.setDataRate( RF24_250KBPS );

        radio.enableAckPayload();

        radio.setRetries(3,5); // delay, count
    } else {
        // Player Controller
        // RF Receiver Setup
        radio.begin();
        radio.setDataRate( RF24_250KBPS );
        radio.openReadingPipe(1, slaveAddresses[BOARD_ID]);

        radio.enableAckPayload();

        radio.startListening();
    }


}

byte prevFive = HIGH;
byte prevOne = HIGH;
byte prevNeg1 = HIGH;
//byte prevOk = HIGH;
int score=0;

//RF Interval variables
unsigned long currentMillis;
unsigned long prevMillis;
unsigned long txIntervalMillis = 10; // send once per second

struct Message {
    Message()
    : senderScore{-1}, receiverScore{-1}, turnNumber{-1}
    {}

    explicit operator bool() const {
        return turnNumber >= 0;
    }

    // senderScore is the current total score for the player that score board knows about
    int senderScore;

    // receiverScore is the new score that the player knows about
    int receiverScore;
    int turnNumber;

    void log(const char* name) const {
        if (!*this) {
            return;
//            Serial.print("<");
//            Serial.print(name);
//            Serial.print("/>");
//            Serial.print("\n");
//            return;
        }
        Serial.print("<");
        Serial.print(name);
        Serial.print(">");
        this->sendToSerial();
        Serial.print("</");
        Serial.print(name);
        Serial.print(">");
        Serial.print("\n");
    }
    private:
    void sendToSerial() const {
        Serial.print("sender=");
        Serial.print(this->senderScore);
        Serial.print(" rcvr=");
        Serial.print(this->receiverScore);
        Serial.print(" turn=");
        Serial.print(this->turnNumber);
    }
};
Message message;

void updatePlayerDisplayScoreLoop(){
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
        message.receiverScore = score;
        message.turnNumber++;
    }

    display.showNumberDec(score, false);
}

//============

Message receiverRF(Message toSend) {
    Message out{};

    if ( ! radio.available() ) {
        return out;
    }
    radio.read( &out, sizeof(out) );

    toSend.log("Sending");
    radio.writeAckPayload(1, &toSend, sizeof(toSend));

    out.log("Received");
    return out;
}

Message senderRF(Message toSend) {
    radio.stopListening();
    radio.openWritingPipe(slaveAddresses[0]);

    Message out{};
    bool rslt;

    toSend.log("ToSlave");
    rslt = radio.write( &toSend, sizeof(toSend) );
    // Always use sizeof() as it gives the size as the number of bytes.
    // For example if dataToSend was an int sizeof() would correctly return 2

    if (rslt) {
        if (radio.isAckPayloadAvailable()) {
            radio.read(&out, sizeof(out));
        }
        out.log("ReceivedFromSenderRf");
    }

    prevMillis = millis();
    return out;
}

void loop() {
    auto mode = BOARD_ID;
    if (mode == -1) {
        // Scoreboard
        currentMillis = millis();
        if (currentMillis - prevMillis >= txIntervalMillis) {
            Message playerMessage = senderRF(message);
            playerMessage.log("PlayerMessage");
            if(playerMessage) {
                if(playerMessage.turnNumber>message.turnNumber) {
                    message.senderScore+=playerMessage.receiverScore;
                    message.turnNumber=playerMessage.turnNumber;
                }
                display.showNumberDec(message.senderScore);
            }
            playerMessage.log("PlayerMessage");
        }
    } else {
        // Console
        updatePlayerDisplayScoreLoop();
        Message scoreboardMessage = receiverRF(message);
        //message = scoreboardMessage;
    }
//    b.update();
//    if (b.fell()) {
//        digitalWrite(LED_BUILTIN, HIGH);
//    }
//    if (b.rose()) {
//        digitalWrite(LED_BUILTIN, LOW);
//    }
}

