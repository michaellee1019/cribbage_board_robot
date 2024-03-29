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

TabletopBoard::TabletopBoard() = default;


struct StateAndLogic {
    StateRefreshRequest lastReceived;
    StateRefreshResponse nextResponse;

public:
    explicit StateAndLogic()
        : lastReceived{},
          nextResponse{} {}

    void update(bool stateUpdate) {
        if (!stateUpdate) {
            return;
        }
        nextResponse.update(lastReceived);
    }
};

class OledDisplay {
    #define SCREEN_WIDTH 128 // OLED display width, in pixels
    #define SCREEN_HEIGHT 32 // OLED display height, in pixels
    #define OLED_RESET    (-1) // Reset pin # (or -1 if sharing Arduino reset pin)
    #define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

    Adafruit_SSD1306 oled{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

public:
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

    int i = 0;
    void addLogLine(const char action[], ScoreT val) {
        oled.setTextSize(1);               // Normal 1:1 pixel scale
        oled.setTextColor(SSD1306_WHITE);  // Draw white text
        oled.cp437(true);                  // Use full 256 char 'Code Page 437' font

        if (i % 4 == 0) {
            oled.clearDisplay();
            oled.setCursor(0, 0);  // Start at top-left corner
        }

        oled.print(i % 10);
        oled.print(" ");
        oled.print(action);
        oled.print(" ");
        oled.print(val);
        oled.println();

        oled.display();
        i++;
    }

    void setup() {
        oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        oled.clearDisplay();
        oled.display();
    }

    void onMessage(const StateAndLogic& logic, bool stateUpdate) {
        if (!stateUpdate) {
            return;
        }
        if (logic.nextResponse.passedTurn()) {
            addLogLine("Passed    ", logic.nextResponse.myScoreDelta());
        }
        if (logic.nextResponse.committed()) {
            addLogLine("Committed ", logic.nextResponse.myScoreDelta());
        }
    }
};

class RotaryEncoder {
    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

public:
    int32_t oldPosition = 0;
    void setup() {
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

    void loop(StateAndLogic& logic) {
        if (!ss.digitalRead(SS_SWITCH)) {
            logic.nextResponse.setCommit(true);
        }
        if (auto newPosition = ScoreT(ss.getEncoderPosition() / 2); oldPosition != newPosition) {
            logic.nextResponse.addScore(newPosition - oldPosition);
            oldPosition = newPosition;
        }
        if (logic.lastReceived.myTurn() || logic.nextResponse.hasScoreDelta()) {
            sspixel.setPixelColor(
                0, OledDisplay::colorWheel((logic.nextResponse.myScoreDelta() * 10) & 0xFF));
            sspixel.setBrightness(10);
            sspixel.show();
        } else {
            sspixel.setPixelColor(0, OledDisplay::colorWheel(0xFF));
            sspixel.setBrightness(1);
            sspixel.show();
        }
    }

};

class Keygrid {
    Button one;
    Button five;
    Button negOne;
    Button passTurn;
    Button commit;
public:
    explicit Keygrid(const IOConfig& config)
    : one{config.pinPlusOne},
      five{config.pinPlusFive},
      negOne{config.pinNegOne},
      passTurn{config.pinPassTurn},
      commit{config.pinCommit} {}

    void setup() const {
        one.setup();
        five.setup();
        negOne.setup();
        passTurn.setup();
        commit.setup();
    }

    void loop(StateAndLogic& logic) {
        five.onLoop([&]() { logic.nextResponse.addScore(5); });
        one.onLoop([&]() { logic.nextResponse.addScore(1); });
        negOne.onLoop([&]() { logic.nextResponse.addScore(-1); });
        commit.onLoop([&]() { logic.nextResponse.setCommit(true); });
        passTurn.onLoop([&]() { logic.nextResponse.setPassTurn(true); });
    }
};

class TurnLight {
    scorebot::view::LEDLight turnLight;
public:
    explicit TurnLight(Light light, bool initialOn)
        : turnLight{light, initialOn} {}
    void setup() const {
        turnLight.setup();
    }
    void loop(const StateAndLogic& logic) {
        if (logic.lastReceived.myTurn()) {
            turnLight.turnOn();
        } else {
            turnLight.turnOff();
        }
        turnLight.update();
    }
};

class MySegmentDisplay {
    scorebot::view::SegmentDisplay segmentDisplay;
public:
    explicit MySegmentDisplay(scorebot::view::SegmentDisplay segmentDisplay)
    : segmentDisplay{segmentDisplay} {}

    void loop(const StateAndLogic& logic) {
        if (logic.lastReceived.myTurn() || logic.nextResponse.hasScoreDelta()) {
            segmentDisplay.setBrightness(0xFF);
        } else {
            segmentDisplay.setBrightness(0xFF / 10);
        }
        segmentDisplay.setValueDec(logic.nextResponse.myScoreDelta());
        segmentDisplay.update();
    }
};

class MyRadio {
    RadioHelper radio;
public:
    explicit MyRadio(RadioHelper radio)
    : radio{radio} {}

    void setup() {
        radio.doRadioSetup();
        radio.openReadingPipe(1, myBoardAddress());
        radio.startListening();
    }

    // TODO: this needs to be more robust
    // See https://www.deviceplus.com/arduino/nrf24l01-rf-module-tutorial/
    [[nodiscard]]
    bool checkForMessages(StateAndLogic& logic) {
        if (!radio.available()) {
            return false;
        }
        if (!radio.doRead(&logic.lastReceived)) {
            return false;
        }
        if (!radio.doAck(1, &logic.nextResponse)) {
            return false;
        }
        return true;
    }
};

struct PlayerBoard::Impl {
    MyRadio radio;
    OledDisplay oled{};
    RotaryEncoder rotaryEncoder{};
    Keygrid keygrid;
    TurnLight turnLight;
    MySegmentDisplay segmentDisplay;
    StateAndLogic logic;

    explicit Impl(const IOConfig& config, TimestampT)
        : radio{RadioHelper{RF24{config.pinRadioCE, config.pinRadioCSN}}},
          keygrid{config},
          turnLight{Light{config.pinTurnLed}, false},
          segmentDisplay{scorebot::view::SegmentDisplay{{8, 7}}},
          logic{} {}

    void setup() {
        keygrid.setup();
        turnLight.setup();
        radio.setup();
        oled.setup();
        rotaryEncoder.setup();
    }

    void loop() {
        keygrid.loop(this->logic);
        rotaryEncoder.loop(this->logic);
        turnLight.loop(this->logic);
        segmentDisplay.loop(this->logic);

        bool stateUpdate = radio.checkForMessages(this->logic);
        this->oled.onMessage(this->logic, stateUpdate);

        this->logic.update(stateUpdate);
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
