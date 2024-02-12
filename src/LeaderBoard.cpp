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

    explicit Impl(IOConfig config) : config{config} {}

    static constexpr int N_DISPLAYS = 4;

    TM1637Display displays[N_DISPLAYS]{
            TM1637Display(8, 7), TM1637Display(6, 5), TM1637Display(4, 3), TM1637Display(2, 21)};


    void setup() {
        for (size_t i = 0; i < N_DISPLAYS; i++) {
            displays[i].setBrightness(0xff);
            displays[i].showNumberDec(int(i + 1));
        }
        delay(1000);
        for (auto & display : displays) {
            display.clear();
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
    Periodically second{100};
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

LeaderBoard::~LeaderBoard() = default;

LeaderBoard::LeaderBoard(IOConfig config) : impl{new Impl(config)} {}
void LeaderBoard::setup() {
    impl->setup();
}

void LeaderBoard::loop() {
    impl->loop();
}

