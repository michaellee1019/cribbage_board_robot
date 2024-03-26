#include "RF24.h"
#include "TM1637Display.h"

#include "Message.hpp"
#include "PlayerBoard.hpp"
#include "RadioHelper.hpp"
#include "TabletopBoard.hpp"
#include "Types.hpp"
#include "Utility.hpp"
#include "View.hpp"


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"

TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RF24 radio;

    Button one;
    Button five;
    Button negOne;
    Button passTurn;
    Button commit;

    View::SegmentDisplay display;
    View::LEDLight turnLight;

    StateRefreshRequest lastReceived;
    StateRefreshResponse nextResponse;

    explicit Impl(IOConfig config, TimestampT)
        : radio{config.pinRadioCE, config.pinRadioCSN},
          one{config.pinPlusOne},
          five{config.pinPlusFive},
          negOne{config.pinNegOne},
          passTurn{config.pinPassTurn},
          commit{config.pinCommit},
          display{{8, 7}},
          turnLight{Light{config.pinTurnLed}, false},
          lastReceived{},
          nextResponse{}
          {}

    void setup() {
        one.setup();
        five.setup();
        negOne.setup();
        passTurn.setup();
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
        radio.openReadingPipe(1, myBoardAddress());
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
        commit.onLoop([&]() { nextResponse.setCommit(true); });
        passTurn.onLoop([&]() { nextResponse.setPassTurn(true); });

        if (this->checkForMessages()) {
            this->nextResponse.update(this->lastReceived);

            if (lastReceived.myTurn()) {
                turnLight.turnOn();
            } else {
                turnLight.turnOff();
            }

            if (lastReceived.myTurn() || this->nextResponse.hasScoreDelta()) {
                display.setBrightness(0xFF);
            } else {
                display.setBrightness(0xFF/10);
            }
        }

        display.setValueDec(this->nextResponse.myScoreDelta());

        display.update();
        turnLight.update();
    }
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "MemoryLeak"
PlayerBoard::PlayerBoard(const IOConfig& config, const TimestampT startupGeneration)
: impl{new Impl(config, startupGeneration)} {}
#pragma clang diagnostic pop

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
    impl->setup();
}

void PlayerBoard::loop() {
    impl->loop();
}

// LeaderBoard


#pragma clang diagnostic pop