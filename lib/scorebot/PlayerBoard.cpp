#include "BoardTypes.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"
#include "ArduinoSTL.h"

#include <Adafruit_SSD1306.h>
#include <Adafruit_seesaw.h>
#include <TM1637Display.h>
#include <seesaw_neopixel.h>

TabletopBoard::TabletopBoard() = default;


struct LoopState {
    unsigned long iteration;
    TimestampT now;

    explicit LoopState() : iteration{0}, now{0} {}

    void advance() {
        this->iteration++;
        this->now = millis();
    }
};

struct StateAndLogic {
    StateRefreshRequest lastReceived;
    StateRefreshResponse nextResponse;

public:
    explicit StateAndLogic() : lastReceived{}, nextResponse{} {}

    void prepNextLoop(bool stateUpdate, const LoopState&) {
        if (!stateUpdate) {
            return;
        }
        nextResponse.update(lastReceived);
    }
};

class OledDisplay {
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 32     // OLED display height, in pixels
#define OLED_RESET (-1)      // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

    Adafruit_SSD1306 oled{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

public:
    explicit OledDisplay(const IOConfig&) {}
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

    short i = 0;
    void addLogLine(const char action[], ScoreT val) {
        if (i == 0) {
            oled.clearDisplay();
            oled.setCursor(0, 0);  // Start at top-left corner
        }

        char message[100];
        snprintf(message, 100, "%2d%s%-3d", i, action, val);
        if (i % 3 == 0 && i > 0) {
            oled.println();
        }
        oled.print(message);

        oled.display();
        i = (i + 1) % 12;
    }

    void setup() {
        oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        oled.clearDisplay();
        oled.display();

        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
        oled.cp437(true);
    }

    void updateView(const StateAndLogic& logic, bool stateUpdate, const LoopState&) {
        if (!stateUpdate) {
            return;
        }
        if (logic.nextResponse.passedTurn()) {
            addLogLine("P", logic.nextResponse.myScoreDelta());
        }
        if (logic.nextResponse.committed()) {
            addLogLine("C", logic.nextResponse.myScoreDelta());
        }
    }
};

class RotaryEncoder {
#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SEESAW_ADDR 0x36

    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};
    int32_t oldPosition = 0;

public:
    explicit RotaryEncoder(const IOConfig&) {}
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

#define BLINK_DELAY_MS 100
    TimestampT blinkStart = 0;
    bool turnOn = true;
    void updateView(const StateAndLogic& logic, bool, const LoopState& loopState) {
        if (logic.lastReceived.myTurn() || logic.nextResponse.hasScoreDelta()) {
            sspixel.setPixelColor(
                0, OledDisplay::colorWheel((logic.nextResponse.myScoreDelta() * 10) & 0xFF));
            if (blinkStart + BLINK_DELAY_MS <= loopState.now) {
                if (turnOn) {
                    sspixel.setBrightness(1);
                } else {
                    sspixel.setBrightness(0);
                }
                //                Serial.print(turnOn);
                //                Serial.print(blinkStart);
                //                Serial.print(0);
                //                Serial.print(0);
                //                Serial.print(now);
                //                Serial.println();
                blinkStart = loopState.now;
                turnOn = !turnOn;
            }

            sspixel.show();
        } else {
            sspixel.setPixelColor(0, OledDisplay::colorWheel(0xFF));
            sspixel.setBrightness(1);
            sspixel.show();
        }
    }

    void loop(StateAndLogic& logic, const LoopState&) {
        if (!ss.digitalRead(SS_SWITCH)) {
            logic.nextResponse.setCommit(true);
        }
        if (auto newPosition = ScoreT(ss.getEncoderPosition() / 2); oldPosition != newPosition) {
            logic.nextResponse.addScore(newPosition - oldPosition);
            oldPosition = newPosition;
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

    void loop(StateAndLogic& logic, const LoopState& loopState) {
        five.onLoop([&]() { logic.nextResponse.addScore(5); });
        one.onLoop([&]() { logic.nextResponse.addScore(1); });
        negOne.onLoop([&]() { logic.nextResponse.addScore(-1); });
        commit.onLoop([&]() { logic.nextResponse.setCommit(true); });
        passTurn.onLoop([&]() { logic.nextResponse.setPassTurn(true); });
    }
};

class TurnLight {
    Light turnLight;

public:
    explicit TurnLight(const IOConfig config) : turnLight{config.pinTurnLed} {}
    void setup() const {
        turnLight.setup();
    }
    void updateView(const StateAndLogic& logic, bool, const LoopState&) {
        if (logic.lastReceived.myTurn()) {
            turnLight.turnOn();
        } else {
            turnLight.turnOff();
        }
    }
};

class MySegmentDisplay {
    TM1637Display segmentDisplay;

public:
    explicit MySegmentDisplay(const IOConfig&) : segmentDisplay{8, 7} {}

    void setup() {}

    void updateView(const StateAndLogic& logic, bool, const LoopState&) {
        if (logic.lastReceived.myTurn() || logic.nextResponse.hasScoreDelta()) {
            segmentDisplay.setBrightness(0xFF);
        } else {
            segmentDisplay.setBrightness(0xFF / 10);
        }
        segmentDisplay.showNumberDec(logic.nextResponse.myScoreDelta());
    }
};

class MyRadio {
    RadioHelper radio;

public:
    explicit MyRadio(const IOConfig& config)
        : radio{RadioHelper{RF24{config.pinRadioCE, config.pinRadioCSN}}} {}

    void setup() {
        radio.doRadioSetup();
        radio.openReadingPipe(1, myBoardAddress());
        radio.startListening();
    }

    // TODO: this needs to be more robust
    // See https://www.deviceplus.com/arduino/nrf24l01-rf-module-tutorial/
    [[nodiscard]] bool checkForMessages(StateAndLogic& logic, const LoopState&) {
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
    // Currently (2024-08-25) the hardware OLED
    // display is not connected since it seems
    // to be unreliable.
//    OledDisplay oled;
//    RotaryEncoder rotaryEncoder;
    Keygrid keygrid;
    TurnLight turnLight;
    MySegmentDisplay segmentDisplay;
    StateAndLogic logic;

    explicit Impl(const IOConfig& config, TimestampT)
        : radio{config},
//          oled{config},
//          rotaryEncoder{config},
          keygrid{config},
          turnLight{config},
          segmentDisplay{config},
          logic{} {}

    void setup() {
        radio.setup();
//        oled.setup();
//        rotaryEncoder.setup();
        keygrid.setup();
        turnLight.setup();
        segmentDisplay.setup();
    }

    LoopState loopState;
    void loop() {
        loopState.advance();

        // Update from the outside world.
        bool stateUpdate = radio.checkForMessages(this->logic, loopState);

        // Check for inputs.
        this->keygrid.loop(this->logic, loopState);
//        this->rotaryEncoder.loop(this->logic, loopState);

        // Update Views
//        this->rotaryEncoder.updateView(this->logic, stateUpdate, loopState);
        this->turnLight.updateView(this->logic, stateUpdate, loopState);
        this->segmentDisplay.updateView(this->logic, stateUpdate, loopState);
//        this->oled.updateView(this->logic, stateUpdate, loopState);

        // Wind up to go again.
        this->logic.prepNextLoop(stateUpdate, loopState);
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
