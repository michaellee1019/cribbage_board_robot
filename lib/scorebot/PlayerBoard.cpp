#include "RF24.h"
#include "TM1637Display.h"

#include "TabletopBoard.hpp"
#include "RadioHelper.hpp"
#include "Message.hpp"
#include "Utility.hpp"
#include "PlayerBoard.hpp"


TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RF24 radio;
    TM1637Display display;
    IOConfig config;
    Button one;
    Button five;
    Button negOne;
    Button add;
    Button commit;
    Light turnLight;
    DipSwitches<4> dipSwitches;

    WhatPlayerBoardAcksInResponse state{};

#if BOARD_ID == 0 || BOARD_ID == -1 
    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};
#endif
#if BOARD_ID == 1
    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'B'};
#endif

    explicit Impl(IOConfig config)
        : radio{config.pinRadioCE, config.pinRadioCSN},
          display(8, 7),
          config{config},
          one{config.pinButton3},
          five{config.pinButton2},
          negOne{config.pinButton1},
          add{config.pinButton4},
          commit{config.pinButton0},
          turnLight{config.pinTurnLed},
          dipSwitches{config.pinDip0, config.pinDip1, config.pinDip2, config.pinDip3}
          {}

    void setup() {
        five.setup();
        one.setup();
        negOne.setup();
        add.setup();
        commit.setup();
        turnLight.setup();
        dipSwitches.setup();

        display.setBrightness(0x0f);
        display.showNumberHexEx(dipSwitches.value());
        delay(500);
        display.clear();

        // Turn LED
        turnLight.turnOn();
        delay(250);
        turnLight.turnOff();

        // TODO: move to RadioHelper
        radio.begin();
        // TODO: set power to low
        // radio.setPALevel(RF_PWR_LOW);
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count

        radio.openReadingPipe(1, thisSlaveAddress);
        radio.startListening();

        radio.printPrettyDetails();
    }

    // TODO: this needs to be more robust
    // See https://www.deviceplus.com/arduino/nrf24l01-rf-module-tutorial/
    void checkForMessages(WhatLeaderBoardSendsEverySecond* leaderboardSent,
                          WhatPlayerBoardAcksInResponse* ackToSendBack) {
        if (!radio.available()) {
            return;
        }
        doRead(&this->radio, leaderboardSent);
        doAck(&this->radio, 1, ackToSendBack);
    }

    void loop() {
        five.onLoop([&]() { state.scoreDelta += 5; });
        one.onLoop([&]() { state.scoreDelta++; });
        negOne.onLoop([&]() { state.scoreDelta--; });

        commit.onLoop([&]() { state.commit = true; });

        WhatLeaderBoardSendsEverySecond received{};
        this->checkForMessages(&received, &state);

        bool myTurn = false;
        ScoreT myScore = -1;
        if (received) {
            if (received.whosTurn == BOARD_ID) {
                myTurn = true;
                myScore = received.whosTurnScore;
                digitalWrite(config.pinTurnLed, HIGH);
            } else {
                digitalWrite(config.pinTurnLed, LOW);
            }
            if (state.commit) {
                state.commit = false;
                state.scoreDelta = 0;
            }
        }

        // Sweet jesuses on a pedestal.
        // This is not acceptable code.
        if (myTurn || state.scoreDelta != 0) {
            display.showNumberDec(state.scoreDelta);
            display.setBrightness(0xFF);
        } else {
            if (myScore < 0) {
                display.showNumberHexEx(0xBEEF);
            } else {
                display.showNumberDec(myScore);
            }
            display.setBrightness(0xF0);
        }

    }
};

PlayerBoard::PlayerBoard(const IOConfig& config) : impl{new Impl(config)} {}

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
    impl->setup();
}

void PlayerBoard::loop() {
    impl->loop();
}

// LeaderBoard

