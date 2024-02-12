#include "RF24.h"
#include "TM1637Display.h"
#include <string>

#include "TabletopBoard.hpp"
#include "RadioHelper.hpp"
#include "Message.hpp"
#include "Utility.hpp"
#include "PlayerBoard.hpp"

// Cruft
#define CE_PIN 10
#define CSN_PIN 9


TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RF24 radio{CE_PIN, CSN_PIN};  // Create a Radio
    TM1637Display display;
    IOConfig config;
    Button one;
    Button five;
    Button negOne;
    Button commit;

    WhatPlayerBoardAcksInResponse state{};

    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};


    explicit Impl(IOConfig config)
        : display(8, 7),
          config{config},
          one{config.pinButton0},
          five{config.pinButton1},
          negOne{config.pinButton2},
          commit{config.pinButton3} {}

    void setup() {
        one.setup();
        five.setup();
        negOne.setup();
        commit.setup();

        display.setBrightness(0x0f);

        // Dip Switches
        pinMode(config.pinDip0, INPUT_PULLUP);
        pinMode(config.pinDip1, INPUT_PULLUP);
        pinMode(config.pinDip2, INPUT_PULLUP);
        pinMode(config.pinDip3, INPUT_PULLUP);

        int dipValue = (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 1 |
            (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 2 |
            (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 3 |
            (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 4;
        display.showNumberHexEx(dipValue);
        delay(500);
        display.clear();

        // Turn LED
        pinMode(config.pinTurnLed, OUTPUT);
        digitalWrite(config.pinTurnLed, HIGH);
        delay(250);
        digitalWrite(config.pinTurnLed, LOW);

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count

        radio.openReadingPipe(1, thisSlaveAddress);
        radio.startListening();

        radio.printPrettyDetails();
    }

    void checkForMessages(WhatLeaderBoardSendsEverySecond* leaderboardSent,
                          WhatPlayerBoardAcksInResponse* ackToSendBack) {
        if (!radio.available()) {
            return;
        }
        doRead(&this->radio, leaderboardSent);
        doAck(&this->radio, 1, ackToSendBack);
    }

    void loop() {
        five.update([&]() { state.scoreDelta += 5; });
        one.update([&]() { state.scoreDelta++; });
        negOne.update([&]() { state.scoreDelta--; });

        commit.update([&]() { state.commit = true; });

        WhatLeaderBoardSendsEverySecond received{};
        this->checkForMessages(&received, &state);

        if (received) {
            if (received.whosTurn == BOARD_ID) {
                digitalWrite(config.pinTurnLed, HIGH);
            } else {
                digitalWrite(config.pinTurnLed, LOW);
            }
            if (state.commit) {
                state.commit = false;
                state.scoreDelta = 0;
            }
        }

        display.showNumberDec(state.scoreDelta);
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

