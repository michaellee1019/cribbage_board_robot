// ReSharper disable CppDFAMemoryLeak
#include "BoardTypes.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Types.hpp"
#include "Utility.hpp"
#include "View.hpp"

#include "RF24.h"
#include "TM1637Display.h"


TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RadioHelper radio;

    Button one;
    Button five;
    Button negOne;
    Button passTurn;
    Button commit;

    scorebot::view::SegmentDisplay display;
    scorebot::view::LEDLight turnLight;

    StateRefreshRequest lastReceived;
    StateRefreshResponse nextResponse;

    explicit Impl(const IOConfig& config, TimestampT)
        : radio{{config.pinRadioCE, config.pinRadioCSN}},
          one{config.pinPlusOne},
          five{config.pinPlusFive},
          negOne{config.pinNegOne},
          passTurn{config.pinPassTurn},
          commit{config.pinCommit},
          display{{8, 7}},
          turnLight{Light{config.pinTurnLed}, false},
          lastReceived{},
          nextResponse{} {}

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

        radio.doRadioSetup();
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
        if (!radio.doRead(&lastReceived)) {
            return false;
        }
        if (!radio.doAck(1, &nextResponse)) {
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
                display.setBrightness(0xFF / 10);
            }
        }

        display.setValueDec(this->nextResponse.myScoreDelta());

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
