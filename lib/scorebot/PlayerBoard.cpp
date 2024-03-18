#include "RF24.h"
#include "TM1637Display.h"

#include "TabletopBoard.hpp"
#include "RadioHelper.hpp"
#include "Message.hpp"
#include "Utility.hpp"
#include "PlayerBoard.hpp"


TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct View {
    class SegmentDisplay {
        enum class DisplayMode {
            kDecimal,
            kHex,
            kUnchanged,
            kClear,
        };
        using HexOrDecimal = union {
            int decimalValue;
            int hexValue;
        };

        DisplayMode mode{DisplayMode::kUnchanged};
        HexOrDecimal value{0};
        TM1637Display display;

        bool changedBrightness{false};
        uint8_t brightness{0xFF};

    public:
        explicit SegmentDisplay(TM1637Display&& display)
            : display{display} {}

        void setValueHex(const uint8_t hexValue) {
            this->mode = DisplayMode::kHex;
            this->value.hexValue = hexValue;
        }
        void setValueDec(const int decimalValue) {
            this->mode = DisplayMode::kDecimal;
            this->value.decimalValue = decimalValue;
        }
        void setBrightness(const uint8_t brightness) {
            this->brightness = brightness;
            changedBrightness = true;
        }
        void clear() {

        }
        void update() {
            if(mode == DisplayMode::kDecimal) {
                display.showNumberDec(value.decimalValue);
                this->mode = DisplayMode::kUnchanged;
            } else if (mode == DisplayMode::kHex) {
                display.showNumberHexEx(value.hexValue);
                this->mode = DisplayMode::kUnchanged;
            } else if (mode == DisplayMode::kClear) {
                display.clear();
                this->mode = DisplayMode::kUnchanged;
            }

            if (changedBrightness) {
                display.setBrightness(brightness);
                changedBrightness = false;
            }
        }
    };

    class LEDLight {
        bool lightOn;
        bool changed;
        Light light;

    public:
        LEDLight(Light&& light, bool initialOn)
            : lightOn{initialOn}, changed{true}, light{light} {}

        void setup() const {
            this->light.setup();
        }
        void turnOn() {
            if (!lightOn) {
                this->lightOn = true;
                this->changed = true;
            }
        }
        void turnOff() {
            if (lightOn) {
                this->lightOn = false;
                this->changed = true;
            }
        }

        void update() {
            if (changed) {
                if (this->lightOn) {
                    this->light.turnOn();
                } else {
                    this->light.turnOff();
                }
                changed = false;
            }
        }
    };

    LEDLight turnLed;
    SegmentDisplay display;

    View(LEDLight turnLed, SegmentDisplay display)
        : turnLed{turnLed}, display{display} {}
};


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

    WhatPlayerBoardAcksInResponse state{};

#if BOARD_ID == 0 || BOARD_ID == -1 
    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};
#endif
#if BOARD_ID == 1
    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'B'};
#endif

    explicit Impl(IOConfig config)
        : radio{config.pinRadioCE, config.pinRadioCSN},
          display{{8, 7}},
          config{config},
          one{config.pinButton3},
          five{config.pinButton2},
          negOne{config.pinButton1},
          add{config.pinButton4},
          commit{config.pinButton0},
          turnLight{Light{config.pinTurnLed}, false}
          {}

    void setup() {
        five.setup();
        one.setup();
        negOne.setup();
        add.setup();
        commit.setup();
        turnLight.setup();

        display.setBrightness(0x0f);
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

        if (received && received.whosTurn == BOARD_ID) {
            turnLight.turnOn();
        } else {
            turnLight.turnOff();
        }

        if (received && received.whosTurn == BOARD_ID && state.commit) {
            state.commit = false;
            state.scoreDelta = 0;
        }

        display.setValueDec(state.scoreDelta);

        display.update();
        turnLight.update();
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

