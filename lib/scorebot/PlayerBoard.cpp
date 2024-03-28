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

#include "Adafruit_seesaw.h"
#include <seesaw_neopixel.h>

#define SS_SWITCH        24
#define SS_NEOPIX        6

#define SEESAW_ADDR          0x36

Adafruit_seesaw ss;
seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX, NEO_GRB + NEO_KHZ800);

int32_t oldPosition;

uint32_t colorWheel(byte WheelPos);

TabletopBoard::TabletopBoard() = default;

void rotaryEncoderSetup() {
    if (! ss.begin(SEESAW_ADDR) || ! sspixel.begin(SEESAW_ADDR)) {
        while(1) delay(10);
    }
    sspixel.setBrightness(20);
    sspixel.show();

    // use a pin for the built in encoder switch
    ss.pinMode(SS_SWITCH, INPUT_PULLUP);

    // get starting position
    oldPosition = ss.getEncoderPosition();

    Serial.println("Turning on interrupts");
    delay(10);
    ss.setGPIOInterrupts((uint32_t)1 << SS_SWITCH, 1);
    ss.enableEncoderInterrupt();
}

uint32_t colorWheel(byte WheelPos) {
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
        rotaryEncoderSetup();
        Serial.println("Started Rotary Encoder");
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

        auto newPosition = ss.getEncoderPosition() / 2;

        if (!ss.digitalRead(SS_SWITCH)) {
            nextResponse.setCommit(true);
        }
        if (oldPosition != newPosition) {
            this->nextResponse.addScore(newPosition - oldPosition);
            oldPosition = newPosition;
        }

        if (lastReceived.myTurn() || this->nextResponse.hasScoreDelta()) {
            sspixel.setPixelColor(0, colorWheel((nextResponse.myScoreDelta() * 10) & 0xFF));
            sspixel.setBrightness(10);
            sspixel.show();
        } else {
            sspixel.setPixelColor(0, colorWheel(0xFF));
            sspixel.setBrightness(1);
            sspixel.show();
        }

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

        display.setValueDec(this->nextResponse.myScoreDelta());

        display.update();
        turnLight.update();

        if (this->checkForMessages()) {
            if (this->nextResponse.passedTurn()) {
                addLogLine("Passed    ", this->nextResponse.myScoreDelta());
            }
            if (this->nextResponse.committed()) {
                addLogLine("Committed ", this->nextResponse.myScoreDelta());
            }
            this->nextResponse.update(this->lastReceived);
        }

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
