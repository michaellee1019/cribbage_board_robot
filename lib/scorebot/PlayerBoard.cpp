// ReSharper disable CppDFAMemoryLeak
#include "BoardTypes.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Types.hpp"
#include "Utility.hpp"
#include "View.hpp"

#include "RF24.h"
#include "TM1637Display.h"


#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RadioHelper radio;
    Adafruit_SSD1306 oled{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

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

    int i = 0;
    void addLogLine(const char action[], ScoreT val) {
        oled.setTextSize(1);      // Normal 1:1 pixel scale
        oled.setTextColor(SSD1306_WHITE); // Draw white text
        oled.cp437(true);         // Use full 256 char 'Code Page 437' font

        if (i % 4 == 0) {
            oled.clearDisplay();
            oled.setCursor(0, 0);     // Start at top-left corner
        }

        oled.print(i%10);
        oled.print(" ");
        oled.print(action);
        oled.print(" ");
        oled.print(val);
        oled.println();

        oled.display();
        i++;
    }

    void setup() {
        one.setup();
        five.setup();
        negOne.setup();
        passTurn.setup();
        commit.setup();

        turnLight.setup();

        display.setBrightness(0x0f);
        display.update();
        delay(10);
        display.clear();
        display.update();

        // Turn LED
        turnLight.turnOn();
        turnLight.update();
        delay(10);
        turnLight.turnOff();
        turnLight.update();

        radio.doRadioSetup();
        radio.openReadingPipe(1, myBoardAddress());
        radio.startListening();

//        radio.printPrettyDetails();
        Serial.println("Started Radio");

        oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        oled.clearDisplay();
        oled.display();
        Serial.println("Started OLED");
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
            if (this->nextResponse.passedTurn()) {
                addLogLine("Passed    ",this->nextResponse.myScoreDelta());
            }
            if (this->nextResponse.committed()) {
                addLogLine("Committed ",this->nextResponse.myScoreDelta());
            }

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
