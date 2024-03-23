#include "LeaderBoard.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"

#include "RF24.h"
#include "TM1637Display.h"

struct LeaderBoard::Impl {
    RF24 radio;
    const IOConfig& config;
    WhatLeaderBoardSendsEverySecond toSend{};

    explicit Impl(const IOConfig& config)
    : radio{config.pinRadioCE, config.pinRadioCSN},
    config{config} {
        toSend.whosTurn = 0;
        toSend.turnNumber = 1;
    }

    // TODO: Make this configurable.
    static constexpr int N_DISPLAYS = 2;

    TM1637Display displays[N_DISPLAYS] {
        // TODO: Put these pin numbers into IOConfig.
        TM1637Display(6, 5),
        TM1637Display(8, 7),
    };


    template<typename F>
    void eachDisplay(F&& callback) {
        for (size_t i = 0; i < N_DISPLAYS; ++i) {
            callback(displays[i], i);
        }
    }

    void setup() {
        eachDisplay([](TM1637Display& display, int i) {
            display.setBrightness(0xFF);
            display.showNumberDec(i + 1);
        });
        delay(500);

        eachDisplay([](TM1637Display& display, int i) {
            display.clear();
        });

        radio.begin();
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count

        radio.openWritingPipe(reinterpret_cast<const uint8_t*>("RxAAA"));

        radio.printPrettyDetails();
    }



    bool send(WhatLeaderBoardSendsEverySecond* toSendV,
              WhatPlayerBoardAcksInResponse* ackReceived) {
        return doSend(&this->radio, toSendV, [&]() { doRead(&this->radio, ackReceived); });
    }

    const byte slaveAddress0[5] = {'R', 'x', 'A', 'A', 'A'};
    const byte slaveAddress1[5] = {'R', 'x', 'A', 'A', 'B'};

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
                    toSend.turnNumber++;
                }
            }
            // for (int i=0; i<N_DISPLAYS;i++) {
            //     if (toSend.whosTurn == i) {
            //         displays[i].setBrightness(0xF0);
            //     } else {
            //         displays[i].setBrightness(0xFF);
            //     }
            // }
        });


    }
};

LeaderBoard::~LeaderBoard() = default;

LeaderBoard::LeaderBoard(const IOConfig& config) : impl{new Impl(config)} {}
void LeaderBoard::setup() {
    impl->setup();
}

void LeaderBoard::loop() {
    impl->loop();
}

