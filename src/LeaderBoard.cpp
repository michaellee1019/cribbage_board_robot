#include "LeaderBoard.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"

#include "RF24.h"
#include "TM1637Display.h"

// Cruft
#define CE_PIN 10
#define CSN_PIN 9

struct LeaderBoard::Impl {
    RF24 radio{CE_PIN, CSN_PIN};
    IOConfig config;
    WhatLeaderBoardSendsEverySecond toSend{};

    explicit Impl(IOConfig config) : config{config} {
        toSend.whosTurn = 0;
        toSend.turnNumber = 1;
    }

    static constexpr int N_DISPLAYS = 2;

    TM1637Display displays[N_DISPLAYS]{
        // TM1637Display(8, 7),
        TM1637Display(6, 5),
        TM1637Display(4, 3),
        // TM1637Display(2, 21)
    };

    void setup() {
        for (size_t i = 0; i < N_DISPLAYS; i++) {
            displays[i].setBrightness(0xff);
            displays[i].showNumberDec(int(i + 1));
        }
        delay(1000);
        for (auto& display : displays) {
            display.clear();
        }

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count

        radio.printPrettyDetails();
    }

    const byte slaveAddress0[5] = {'R', 'x', 'A', 'A', 'A'};
    const byte slaveAddress1[5] = {'R', 'x', 'A', 'A', 'B'};

    bool send(WhatLeaderBoardSendsEverySecond* toSendV,
              WhatPlayerBoardAcksInResponse* ackReceived) {
        return doSend(&this->radio, toSendV, [&]() { doRead(&this->radio, ackReceived); });
    }

    ScoreT player0 = 0;
    ScoreT player1 = 0;
    Periodically second{100};
    void loop() {  // Leaderboard
        second.run(millis(), [&]() {
            WhatPlayerBoardAcksInResponse ack0{};
            this->radio.stopListening();
            this->radio.openWritingPipe(slaveAddress0);
            this->send(&toSend, &ack0);
            if (ack0.commit) {
                player0 += ack0.scoreDelta;
                this->displays[0].showNumberDec(player0);
                if (toSend.whosTurn == 0) {
                    toSend.whosTurn = (toSend.whosTurn + 1) % N_DISPLAYS;
                    toSend.whosTurnScore = player0;
                    toSend.turnNumber++;
                }
            }
            WhatPlayerBoardAcksInResponse ack1{};
            this->radio.stopListening();
            this->radio.openWritingPipe(slaveAddress1);
            this->send(&toSend, &ack1);
            if (ack1.commit) {
                player1 += ack1.scoreDelta;
                this->displays[1].showNumberDec(player1);
                if (toSend.whosTurn == 1) {
                    toSend.whosTurn = (toSend.whosTurn + 1) % N_DISPLAYS;
                    toSend.whosTurnScore = player1;
                    toSend.turnNumber++;
                }
            }
        });
    }
};

LeaderBoard::~LeaderBoard() = default;

LeaderBoard::LeaderBoard(IOConfig config) : impl{new Impl(config)} {}
void LeaderBoard::setup() {
    impl->setup();
}

void LeaderBoard::loop() {
    impl->loop();
}
