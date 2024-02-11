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


struct Button {
    int pin;
    explicit Button(int pin) : pin{pin} {}
    byte previous = HIGH;
    void setup() const {
        pinMode(pin, INPUT_PULLUP);
    }
    template <typename F>
    void update(F&& onPress) {
        byte state = digitalRead(pin);
        if (state == LOW && state != previous) {
            onPress();
        }
        previous = state;
    }
};

struct Periodically {
    const int period;
    explicit Periodically(int period) : period{period} {}
    unsigned long lastSent{0};
    template <typename F>
    void run(unsigned long now, F&& callback) {
        if (now >= lastSent + period) {
            callback();
            lastSent = now;
        }
    }
};


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

    WhatLeaderBoardSendsEverySecond toSend{};
    ScoreT player0 = 0;
    Periodically second{1000};
    void loop() {  // Leaderboard
        second.run(millis(), [&]() {
            WhatPlayerBoardAcksInResponse ack{};
            this->send(&toSend, &ack);
            if (ack.commit) {
                player0 += ack.scoreDelta;
                this->displays[2].showNumberDec(player0);
                toSend.whosTurn = (toSend.whosTurn + 1) % N_DISPLAYS;
                toSend.turnNumber++;
            }
        });
        this->displays[1].showNumberDec(toSend.turnNumber);
    }
};

TabletopBoard::TabletopBoard() = default;

// PlayerBoard

struct PlayerBoard::Impl {
    RF24 radio{CE_PIN, CSN_PIN};  // Create a Radio
    TM1637Display display;
    IOConfig config;
    Button one;
    Button five;
    Button negOne;
    Button commit;

    WhatPlayerBoardAcksInResponse state{};

    const byte thisSlaveAddress[5] = {'R', 'x', 'A', 'A', 'A'};


    explicit Impl(IOConfig config)
        : display(8, 7),
          config{config},
          one{config.pinButton0},
          five{config.pinButton1},
          negOne{config.pinButton2},
          commit{config.pinButton3} {}

    void setup() {
        one.setup();
        five.setup();
        negOne.setup();
        commit.setup();

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
        display.showNumberHexEx(dipValue);
        delay(500);
        display.clear();

        // Turn LED
        pinMode(config.pinTurnLed, OUTPUT);
        digitalWrite(config.pinTurnLed, HIGH);
        delay(250);
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

    void loop() {
        five.update([&]() { state.scoreDelta += 5; });
        one.update([&]() { state.scoreDelta++; });
        negOne.update([&]() { state.scoreDelta--; });

        commit.update([&]() { state.commit = true; });

        WhatLeaderBoardSendsEverySecond received{};
        this->checkForMessages(&received, &state);

        if (received) {
            if (received.whosTurn == BOARD_ID) {
                digitalWrite(config.pinTurnLed, HIGH);
            } else {
                digitalWrite(config.pinTurnLed, LOW);
            }
            if (state.commit) {
                state.commit = false;
                state.scoreDelta = 0;
            }
        }

        display.showNumberDec(state.scoreDelta);
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
