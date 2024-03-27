// ReSharper disable CppDFAMemoryLeak
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

#include "Adafruit_seesaw.h"
#include <seesaw_neopixel.h>

#define SS_SWITCH        24
#define SS_NEOPIX        6

#define SEESAW_ADDR          0x36

Adafruit_seesaw ss;
seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX, NEO_GRB + NEO_KHZ800);

int32_t encoder_position;

uint32_t Wheel(byte WheelPos);

void rotaryEncoderSetup() {
    if (! ss.begin(SEESAW_ADDR) || ! sspixel.begin(SEESAW_ADDR)) {
        while(1) delay(10);
    }
    sspixel.setBrightness(20);
    sspixel.show();

    // use a pin for the built in encoder switch
    ss.pinMode(SS_SWITCH, INPUT_PULLUP);

    // get starting position
    encoder_position = ss.getEncoderPosition();

    Serial.println("Turning on interrupts");
    delay(10);
    ss.setGPIOInterrupts((uint32_t)1 << SS_SWITCH, 1);
    ss.enableEncoderInterrupt();
}

void loop2() {
    if (! ss.digitalRead(SS_SWITCH)) {
        Serial.println("Button pressed!");
    }

    int32_t new_position = ss.getEncoderPosition();
    // did we move arounde?
    if (encoder_position != new_position) {
        Serial.println(new_position);         // display new position

        // change the neopixel color
        sspixel.setPixelColor(0, Wheel(new_position & 0xFF));
        sspixel.show();
        encoder_position = new_position;      // and save for next round
    }

    // don't overwhelm serial port
    delay(10);
}


uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return sspixel.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return sspixel.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return sspixel.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

struct PlayerBoard::Impl {
    RF24 radio;

    Button one;
    Button five;
    Button negOne;
    Button passTurn;
    Button commit;

    SegmentDisplay display;
    LEDLight turnLight;

    StateRefreshRequest lastReceived;
    StateRefreshResponse nextResponse;

    explicit Impl(const IOConfig& config, TimestampT)
        : radio{config.pinRadioCE, config.pinRadioCSN},
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

        doRadioSetup(radio);
        radio.openReadingPipe(1, myBoardAddress());
        radio.startListening();

        radio.printPrettyDetails();
        rotaryEncoderSetup();
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
        loop2();
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
