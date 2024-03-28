// ReSharper disable CppDFAMemoryLeak
#include "BoardTypes.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Types.hpp"
#include "Utility.hpp"
#include "View.hpp"

#include <Adafruit_SSD1306.h>
#include "Adafruit_seesaw.h"
#include <seesaw_neopixel.h>

// Rotary Encoder
#define SS_SWITCH        24
#define SS_NEOPIX        6
#define SEESAW_ADDR          0x36

// OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


TabletopBoard::TabletopBoard() = default;

struct PlayerBoard::Impl {
    RadioHelper radio;

    Adafruit_SSD1306 oled{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};
    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX, NEO_GRB + NEO_KHZ800);

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

    int32_t oldPosition = 0;
    void doRotaryEncoderSetup() {
        ss.begin(SEESAW_ADDR);
        sspixel.begin(SEESAW_ADDR);
        sspixel.setBrightness(20);
        sspixel.show();

        ss.pinMode(SS_SWITCH, INPUT_PULLUP);

        // get starting position
        oldPosition = ss.getEncoderPosition();

        ss.setGPIOInterrupts((uint32_t)1 << SS_SWITCH, true);
        ss.enableEncoderInterrupt();
    }

    static uint32_t colorWheel(byte WheelPos) {
        WheelPos = 255 - WheelPos;
        if (WheelPos < 85) {
            return seesaw_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
        }
        if (WheelPos < 170) {
            WheelPos -= 85;
            return seesaw_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
        }
        WheelPos -= 170;
        return seesaw_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }

    void setup() {
        this->doKeyGridSetup();
        this->doTurnLightSetup();
        this->doRadioSetup();
        this->doOledSetup();
        this->doRotaryEncoderSetup();
    }
    void doKeyGridSetup() const {
        one.setup();
        five.setup();
        negOne.setup();
        passTurn.setup();
        commit.setup();
    }
    void doTurnLightSetup() const {
        turnLight.setup();
    }
    void doOledSetup() {
        oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        oled.clearDisplay();
        oled.display();
    }
    void doRadioSetup() {
        radio.doRadioSetup();
        radio.openReadingPipe(1, myBoardAddress());
        radio.startListening();
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

    void loopKeygrid() {
        five.onLoop([&]() { nextResponse.addScore(5); });
        one.onLoop([&]() { nextResponse.addScore(1); });
        negOne.onLoop([&]() { nextResponse.addScore(-1); });
        commit.onLoop([&]() { nextResponse.setCommit(true); });
        passTurn.onLoop([&]() { nextResponse.setPassTurn(true); });
    }

    void loop() {
        this->loopKeygrid();
        this->loopRotaryEncoder();
        this->loopTurnLight();
        this->loopSegmentDisplay();


        if (this->checkForMessages()) {
            this->loopOledDisplay_onMessage();
            this->loopLogic_onMessage();
            this->loopLogic_onMessage();
        }

    }
    void loopLogic_onMessage() {
        nextResponse.update(lastReceived);
    }
    void loopOledDisplay_onMessage() {
        if (nextResponse.passedTurn()) {
            addLogLine("Passed    ", nextResponse.myScoreDelta());
        }
        if (nextResponse.committed()) {
            addLogLine("Committed ", nextResponse.myScoreDelta());
        }
    }
    void loopSegmentDisplay() {
        if (lastReceived.myTurn() || nextResponse.hasScoreDelta()) {
            display.setBrightness(0xFF);
        } else {
            display.setBrightness(0xFF / 10);
        }
        display.setValueDec(nextResponse.myScoreDelta());
        display.update();
    }
    void loopTurnLight() {
        if (lastReceived.myTurn()) {
            turnLight.turnOn();
        } else {
            turnLight.turnOff();
        }
        turnLight.update();
    }
    void loopRotaryEncoder() {
        if (!ss.digitalRead(SS_SWITCH)) {
            nextResponse.setCommit(true);
        }
        if (auto newPosition = ScoreT(ss.getEncoderPosition() / 2); oldPosition != newPosition) {
            nextResponse.addScore(newPosition - oldPosition);
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
