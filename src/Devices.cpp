#include "RF24.h"
#include "TM1637Display.h"
#include <string>

#include <scorebot/Devices.hpp>
#include <scorebot/LeaderBoard.hpp>
#include <scorebot/Message.hpp>
#include <scorebot/PlayerBoard.hpp>

// Cruft
#define CE_PIN 10
#define CSN_PIN 9

struct LeaderBoard::Impl {
    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};
    RF24 radio{CE_PIN, CSN_PIN};
    IOConfig config;

    explicit Impl(IOConfig config) : config{config} {}

    TM1637Display displays[4]{
        TM1637Display(8, 7), TM1637Display(6, 5), TM1637Display(4, 3), TM1637Display(2, 21)};


    void setup() {
        Serial.println("setup started");
        uint8_t fullDisplay[] = {0xff, 0xff, 0xff, 0xff};
        for (size_t i = 0; i < 4; i++) {
            Serial.println("init display");
            displays[i].setBrightness(0x0f);
            displays[i].setSegments(fullDisplay);
            delay(100);
            displays[i].showNumberDec(int(i + 1));
            delay(100);
        }

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.openReadingPipe(1, thisSlaveAddress);

        radio.enableAckPayload();

        radio.startListening();

        WhatLeaderBoardSendsEverySecond ack{};
        radio.writeAckPayload(1, &ack, sizeof(ack));  // pre-load data

        radio.printPrettyDetails();
    }

    unsigned long lastSent = 0;

    void send(WhatPlayerBoardSends* toSend, WhatLeaderBoardSendsEverySecond* ack) {
        bool rslt = this->radio.write(toSend, sizeof(WhatPlayerBoardSends));
        if (rslt) {
            if (radio.isAckPayloadAvailable()) {
                radio.read(ack, sizeof(WhatLeaderBoardSendsEverySecond));
            }
        }
    }

    void loop() {
        auto time = millis();
        if (time - lastSent >= 1000) {
            WhatPlayerBoardSends state{};
            WhatLeaderBoardSendsEverySecond ack{};
            this->send(&state, &ack);
            state.log("Sent");
            lastSent = time;
        }
    }
};

TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    const byte slaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};
    RF24 radio{CE_PIN, CSN_PIN};  // Create a Radio

    TM1637Display display;
    IOConfig config;
    byte prevFive = HIGH;
    byte prevOne = HIGH;
    byte prevNeg1 = HIGH;
    //    byte prevOk = HIGH;
    short score = 0;

    explicit Impl(IOConfig config) : display(8, 7), config{config} {}

    void setup() {
        pinMode(config.pinButton0, INPUT_PULLUP);
        pinMode(config.pinButton1, INPUT_PULLUP);
        pinMode(config.pinButton2, INPUT_PULLUP);
        pinMode(config.pinButton3, INPUT_PULLUP);

        // pull up resistor
        digitalWrite(config.pinButton0, HIGH);
        digitalWrite(config.pinButton1, HIGH);
        digitalWrite(config.pinButton2, HIGH);
        digitalWrite(config.pinButton3, HIGH);
        display.setBrightness(0x0f);

        uint8_t fullDisplay[] = {0xff, 0xff, 0xff, 0xff};
        //        uint8_t blankDisplay[] = {0x00, 0x00, 0x00, 0x00};

        display.setSegments(fullDisplay);
        delay(500);

        // dipSwitch
        pinMode(14, INPUT_PULLUP);
        pinMode(15, INPUT_PULLUP);
        pinMode(16, INPUT_PULLUP);
        pinMode(17, INPUT_PULLUP);

        // pull up resistor
        digitalWrite(14, HIGH);
        digitalWrite(15, HIGH);
        digitalWrite(16, HIGH);
        digitalWrite(17, HIGH);

        uint8_t zeroDigit = display.encodeDigit(0);
        uint8_t oneDigit = display.encodeDigit(1);
        uint8_t dipDisplayBits[] = {zeroDigit, zeroDigit, zeroDigit, zeroDigit};
        if (digitalRead(14) == LOW) {
            dipDisplayBits[0] = oneDigit;
        }
        if (digitalRead(15) == LOW) {
            dipDisplayBits[1] = oneDigit;
        }
        if (digitalRead(16) == LOW) {
            dipDisplayBits[2] = oneDigit;
        }
        if (digitalRead(17) == LOW) {
            dipDisplayBits[3] = oneDigit;
        }

        display.setSegments(dipDisplayBits);
        delay(500);

        pinMode(2, OUTPUT);
        digitalWrite(2, HIGH);
        delay(500);
        digitalWrite(2, LOW);

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count
        // TODO: 5 gives a 1500 µsec delay which is needed for a 32 byte ackPayload
        radio.openWritingPipe(slaveAddress);

        radio.printPrettyDetails();
    }

    void checkForMessages(WhatPlayerBoardSends* received, WhatLeaderBoardSendsEverySecond* ack) {
        if (!radio.available()) {
            return;
        }
        radio.read(received, sizeof(WhatPlayerBoardSends));
        display.showNumberDec(received->iThinkItsNowTurnNumber);
        // TODO: The comment below makes no sense after all these refactorings.
        //       Did I do this right? What is "the next time"? It sounds ominous.
        radio.writeAckPayload(
                1, ack, sizeof(WhatLeaderBoardSendsEverySecond));  // load the payload for the next time
    }


    void loop() {
        WhatPlayerBoardSends received{};
        WhatLeaderBoardSendsEverySecond ack{};
        this->checkForMessages(&received, &ack);
        received.log("Received");

        // TODO: populate ack with the result of the below logic

        // 5pt
        byte fiveState = digitalRead(config.pinButton0);
        if (fiveState == LOW && fiveState != prevFive) {
            score += 5;
        }
        prevFive = fiveState;

        // 1pt
        byte oneState = digitalRead(config.pinButton1);
        if (oneState == LOW && oneState != prevOne) {
            score++;
        }
        prevOne = oneState;

        // -1pt
        byte neg1State = digitalRead(config.pinButton2);
        if (neg1State == LOW && neg1State != prevNeg1) {
            score -= 1;
        }
        prevNeg1 = neg1State;

        // commit
        if (digitalRead(config.pinButton3) == LOW) {
            for (byte i = 0; i < 2; i++) {
                delay(100);
                display.clear();
                delay(100);
                display.showNumberDec(score, false);
            }
            score = 0;
        }

        display.showNumberDec(score, false);
    }
};

PlayerBoard::PlayerBoard(IOConfig config) : impl{new Impl(config)} {}

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
    impl->setup();
}

void PlayerBoard::loop() {
    impl->loop();
}

// LeaderBoard

LeaderBoard::~LeaderBoard() = default;

LeaderBoard::LeaderBoard(IOConfig config) : impl{new Impl(config)} {}
void LeaderBoard::setup() {
    impl->setup();
}

void LeaderBoard::loop() {
    impl->loop();
}
