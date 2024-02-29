#include "LeaderBoard.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"

#include "RF24.h"
#include "TM1637Display.h"

// TODO: Put these pin numbers into IOConfig.
#define CE_PIN 10
#define CSN_PIN 9

struct LeaderBoard::Impl {
    RF24 radio;
    IOConfig config;

    explicit Impl(IOConfig config)
    : radio{CE_PIN, CSN_PIN},
      config{config}
    {}

    // TODO: Make this configurable.
    static constexpr int N_DISPLAYS = 4;

    TM1637Display displays[N_DISPLAYS] {
        // TODO: Put these pin numbers into IOConfig.
        TM1637Display(8, 7),
        TM1637Display(6, 5),
        TM1637Display(4, 3),
        TM1637Display(2, 21)
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

