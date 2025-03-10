// ReSharper disable CppDFAMemoryLeak
#include <iostream>
#include "BoardTypes.hpp"
#include "Message.hpp"
#include "Utility.hpp"

#include "Adafruit_FRAM_I2C.h"
#include "TM1637Display.h"

struct LeaderBoard::Impl {
    Adafruit_FRAM_I2C storage;

    TM1637Display displays[MAX_DISPLAYS]{
        // TODO: Put these pin numbers into IOConfig.
        TM1637Display(6, 5),
        TM1637Display(8, 7),
        TM1637Display(4, 3),
//        TM1637Display(14, 2),
    };

    StateRefreshRequest nextRequest;
    StateRefreshResponse lastResponses[MAX_PLAYERS];

    explicit Impl(const IOConfig& config, TimestampT)
        : nextRequest{} {}


    template <typename F>
    void eachDisplay(F&& callback) {
        for (size_t i = 0; i < MAX_DISPLAYS; ++i) {
            callback(displays[i], i);
        }
    }

    void displayStartup() {
        eachDisplay([](TM1637Display& display, int i) {
            display.setBrightness(0xFF);
            display.showNumberDec(i + 1);
        });
        delay(500);
        eachDisplay([](TM1637Display& display, int i) { display.clear(); });
    }


    void setup() {
        storage.begin(MB85RC_DEFAULT_ADDRESS);
        storage.readObject(0x00, nextRequest);

        this->displayStartup();

//        radio.doRadioSetup();
//        radio.printPrettyDetails();

        eachDisplay([&](TM1637Display& display, int i) { display.showNumberDec(i); });
    }


    bool send(StateRefreshResponse* outputResponse) {
        return false;
//        return radio.doSend(&nextRequest, [&]() { return radio.doRead(outputResponse); });
    }

    Periodically everySecond{1000};
    PlayerNumberT maxActivePlayerIndex = 0;
    void loop() {  // Leaderboard

        everySecond.run(millis(), [&]() {
            for (PlayerNumberT i = 0; i < MAX_PLAYERS; ++i) {
                const auto addr = playerAddress(i);
//                this->radio.stopListening();
//                this->radio.openWritingPipe(addr.value());
                const auto recv = this->send(&lastResponses[i]);
                std::cout << "Player " << i << addr
                          << "=> " << lastResponses[i]
                          << " ? " << (recv ? "T" : "F")
                          << std::endl;
                if (recv) {
                    this->maxActivePlayerIndex = max(this->maxActivePlayerIndex, i);
                }
            }
            this->nextRequest.update(lastResponses, MAX_PLAYERS, maxActivePlayerIndex);

            eachDisplay([&](TM1637Display& display, int i) {
                if (i > maxActivePlayerIndex) {
                    display.showNumberHexEx(0xDEAD);
                    display.setBrightness(0x01);
                } else {
                    display.showNumberDec(nextRequest.getPlayerScore(i));
                    display.setBrightness(i == nextRequest.whosTurn() ? 0xFF : 0xFF / 10);
                }
            });
        });
    }
};

LeaderBoard::~LeaderBoard() = default;

LeaderBoard::LeaderBoard(const IOConfig& config, TimestampT startupGeneration)
    : impl{new Impl(config, startupGeneration)} {}
void LeaderBoard::setup() {
    impl->setup();
}

void LeaderBoard::loop() {
    impl->loop();
}
