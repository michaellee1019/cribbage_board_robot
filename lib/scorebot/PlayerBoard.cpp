#include "RF24.h"
#include "TM1637Display.h"

#include "Message.hpp"
#include "PlayerBoard.hpp"
#include "RadioHelper.hpp"
#include "TabletopBoard.hpp"
#include "Types.hpp"
#include "Utility.hpp"
#include "View.hpp"


TabletopBoard::TabletopBoard() = default;

// PlayerBoard

#if BOARD_ID == 0 || BOARD_ID == -1
const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};
#endif
#if BOARD_ID == 1
const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'B'};
#endif
#if BOARD_ID == 2
const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'C'};
#endif


struct PlayerBoard::Impl {
    RF24 radio;
    View::SegmentDisplay display;
    IOConfig config;
    Button one;
    Button five;
    Button negOne;
    Button add;
    Button commit;
    View::LEDLight turnLight;

    StateRefreshRequest lastReceived;
    StateRefreshResponse nextResponse;

    explicit Impl(IOConfig config, TimestampT startupGeneration)
        : radio{config.pinRadioCE, config.pinRadioCSN},
          display{{8, 7}},
          config{config},
          one{config.pinButton3},
          five{config.pinButton2},
          negOne{config.pinButton1},
          add{config.pinButton4},
          commit{config.pinButton0},
          turnLight{Light{config.pinTurnLed}, false},
          lastReceived{startupGeneration},
          nextResponse{false, BOARD_ID, 0, false, false, {}}
          {}

    void setup() {
        five.setup();
        one.setup();
        negOne.setup();
        add.setup();
        commit.setup();
        turnLight.setup();

        display.setBrightness(0x0f);
        display.update();
        delay(500);
        display.clear();
        display.update();

        // Turn LED
        turnLight.turnOn();
        turnLight.update();
        delay(250);
        turnLight.turnOff();
        turnLight.update();

        doRadioSetup(radio);
        radio.openReadingPipe(1, thisSlaveAddress);
        radio.startListening();

        radio.printPrettyDetails();
    }

    // TODO: this needs to be more robust
    // See https://www.deviceplus.com/arduino/nrf24l01-rf-module-tutorial/
    [[nodiscard]]
    bool checkForMessages() {
        if (!radio.available()) {
            return false;
        }
        if (!doRead(&this->radio, &lastReceived)) {
            return false;
        }
        if (!doAck(&this->radio, 1, &nextResponse)) {
            return false;
        }
        return true;
    }

    void loop() {
        five.onLoop([&]() { nextResponse.addScore(5); });
        one.onLoop([&]() { nextResponse.addScore(1); });
        negOne.onLoop([&]() { nextResponse.addScore(-1); });
        commit.onLoop([&]() { nextResponse.commit = true; });

        if (this->checkForMessages()) {
            this->nextResponse.update(this->lastReceived);

            if (lastReceived.myTurn()) {
                turnLight.turnOn();
                display.setBrightness(0xFF);
            } else {
                turnLight.turnOff();
                display.setBrightness(0xFF / 10);
            }
        }

        display.setValueDec(this->nextResponse.delta.scoreDelta);

        display.update();
        turnLight.update();
    }
};

PlayerBoard::PlayerBoard(const IOConfig& config, const TimestampT startupGeneration)
: impl{new Impl(config, startupGeneration)} {}

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
    impl->setup();
}

void PlayerBoard::loop() {
    impl->loop();
}

// LeaderBoard

