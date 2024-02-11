#include "RF24.h"
#include "TM1637Display.h"
#include <string>

#include <scorebot/Devices.hpp>
#include <scorebot/LeaderBoard.hpp>
#include <scorebot/Message.hpp>
#include <scorebot/PlayerBoard.hpp>

// Cruft
#define CE_PIN 10
#define CSN_PIN 9


namespace {
// Send toSend and invoke callback if successfully sent.
template <typename T, typename F>
[[nodiscard]] bool doSend(RF24* radio, T* toSend, F&& callback) {
    bool sent = radio->write(toSend, sizeof(T));
    if (sent) {
        callback();
    }
    return sent;
}

template <typename T>
bool doRead(RF24* radio, T* out) {
    if (radio->isAckPayloadAvailable()) {
        radio->read(out, sizeof(T));
        return true;
    }
    return false;
}

template <typename T>
void doAck(RF24* radio, uint8_t pipe, T* acked) {
    radio->writeAckPayload(pipe, acked, sizeof(T));
}
}  // namespace

struct LeaderBoard::Impl {
    RF24 radio{CE_PIN, CSN_PIN};
    IOConfig config;

    explicit Impl(IOConfig config) : config{config} {}

    static constexpr int N_DISPLAYS = 4;

    TM1637Display displays[N_DISPLAYS]{
        TM1637Display(8, 7), TM1637Display(6, 5), TM1637Display(4, 3), TM1637Display(2, 21)};


    void setup() {
        for (size_t i = 0; i < N_DISPLAYS; i++) {
            displays[i].setBrightness(0xff);
            displays[i].showNumberDec(int(i + 1));
        }

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count

        radio.openWritingPipe(slaveAddress);

        radio.printPrettyDetails();
    }

    const byte slaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};


    bool send(WhatLeaderBoardSendsEverySecond* toSendV,
              WhatPlayerBoardAcksInResponse* ackReceived) {
        return doSend(&this->radio, toSendV, [&]() { doRead(&this->radio, ackReceived); });
    }

    unsigned long lastSent = 0;
    WhatLeaderBoardSendsEverySecond toSend{};
    void loop() {
        auto time = millis();
        if (time - lastSent >= 1000) {
            // TODO: Leaderboard sends an update to each playerboard every second.
            WhatPlayerBoardAcksInResponse ack{};
            this->send(&toSend, &ack);
            toSend.log("Sent");
            if (ack) {
                this->displays[2].showNumberDec(ack.myScore);
                if (ack.advance) {
                    toSend.whosTurn = (toSend.whosTurn + 1) % N_DISPLAYS;
                    toSend.turnNumber++;
                }
                this->displays[1].showNumberDec(toSend.turnNumber);
            }
            lastSent = time;
        }
    }
};

TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RF24 radio{CE_PIN, CSN_PIN};  // Create a Radio
    TM1637Display display;
    IOConfig config;
    byte prevFive = HIGH;
    byte prevOne = HIGH;
    byte prevNeg1 = HIGH;
    //    byte prevOk = HIGH;
    short score = 0;

    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};


    explicit Impl(IOConfig config) : display(8, 7), config{config} {}

    void setup() {
        pinMode(config.pinButton0, INPUT_PULLUP);
        pinMode(config.pinButton1, INPUT_PULLUP);
        pinMode(config.pinButton2, INPUT_PULLUP);
        pinMode(config.pinButton3, INPUT_PULLUP);

        display.setBrightness(0x0f);

        // Dip Switches
        pinMode(config.pinDip0, INPUT_PULLUP);
        pinMode(config.pinDip1, INPUT_PULLUP);
        pinMode(config.pinDip2, INPUT_PULLUP);
        pinMode(config.pinDip3, INPUT_PULLUP);

        int dipValue = (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 1 |
            (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 2 |
            (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 3 |
            (digitalRead(config.pinDip0) == LOW ? 0 : 1) << 4;
        display.showNumberDec(dipValue);
        delay(500);
        display.clear();

        // Turn LED
        pinMode(config.pinTurnLed, OUTPUT);
        digitalWrite(config.pinTurnLed, HIGH);
        delay(500);
        digitalWrite(config.pinTurnLed, LOW);

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count

        radio.openReadingPipe(1, thisSlaveAddress);
        radio.startListening();

        radio.printPrettyDetails();
    }

    void checkForMessages(WhatLeaderBoardSendsEverySecond* leaderboardSent,
                          WhatPlayerBoardAcksInResponse* ackToSendBack) {
        if (!radio.available()) {
            return;
        }
        doRead(&this->radio, leaderboardSent);
        doAck(&this->radio, 1, ackToSendBack);
    }

    bool commit = false;
    void loop() {
        // 5pt
        byte fiveState = digitalRead(config.pinButton0);
        if (fiveState == LOW && fiveState != prevFive) {
            score += 5;
        }
        prevFive = fiveState;

        // 1pt
        byte oneState = digitalRead(config.pinButton1);
        if (oneState == LOW && oneState != prevOne) {
            score++;
        }
        prevOne = oneState;

        // -1pt
        byte neg1State = digitalRead(config.pinButton2);
        if (neg1State == LOW && neg1State != prevNeg1) {
            score -= 1;
        }
        prevNeg1 = neg1State;

        // commit
        if (digitalRead(config.pinButton3) == LOW) {
            commit = true;
        }

        display.showNumberDec(score);

        WhatPlayerBoardAcksInResponse ackResponse{};
        ackResponse.myPlayerNumber = BOARD_ID;
        if (commit) {
            ackResponse.myScore = score;
        }

        WhatLeaderBoardSendsEverySecond received{};
        this->checkForMessages(&received, &ackResponse);

        if (received) {
            if (received.whosTurn == BOARD_ID) {
                digitalWrite(config.pinTurnLed, HIGH);
            }
            if (commit) {
                commit = false;
            }
        }
        delay(100);
    }
};

PlayerBoard::PlayerBoard(IOConfig config) : impl{new Impl(config)} {}

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
    impl->setup();
}

void PlayerBoard::loop() {
    impl->loop();
}

// LeaderBoard

LeaderBoard::~LeaderBoard() = default;

LeaderBoard::LeaderBoard(IOConfig config) : impl{new Impl(config)} {}
void LeaderBoard::setup() {
    impl->setup();
}

void LeaderBoard::loop() {
    impl->loop();
}
