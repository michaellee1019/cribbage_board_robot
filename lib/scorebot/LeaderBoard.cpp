// ReSharper disable CppDFAMemoryLeak
#include "LeaderBoard.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"

#include "Adafruit_FRAM_I2C.h"
#include "PlayerBoard.hpp"
#include "RF24.h"
#include "TM1637Display.h"

struct LeaderBoard::Impl {
    RF24 radio;
    Adafruit_FRAM_I2C storage;

    TM1637Display displays[MAX_DISPLAYS] {
        // TODO: Put these pin numbers into IOConfig.
        TM1637Display(6, 5),
        TM1637Display(8, 7),
        TM1637Display(4, 3),
    };

    StateRefreshRequest nextRequest;
    StateRefreshResponse lastResponses[MAX_PLAYERS];

    explicit Impl(const IOConfig& config, TimestampT)
    : radio{config.pinRadioCE, config.pinRadioCSN},
      nextRequest{} {}


    template<typename F>
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
        eachDisplay([](TM1637Display& display, int i) {
            display.clear();
        });
    }


    void setup() {
        storage.begin(MB85RC_DEFAULT_ADDRESS);
        storage.readObject(0x00, nextRequest);

        this->displayStartup();

        doRadioSetup(radio);
        radio.openWritingPipe(myBoardAddress());
        radio.printPrettyDetails();

        eachDisplay([&](TM1637Display& display, int i) {
            display.showNumberDec(i);
        });

    }


    bool send(StateRefreshResponse* responseReceived) {
        return doSend(&this->radio, &nextRequest, [&]() { return doRead(&this->radio, responseReceived); });
    }

    Periodically everySecond{500};
    PlayerNumberT maxActivePlayerIndex = 0;
    void loop() {  // Leaderboard

        everySecond.run(millis(), [&]() {
            for (PlayerNumberT i=0; i < MAX_PLAYERS; ++i) {
                this->radio.stopListening();
                this->radio.openWritingPipe(playerAddress(i));

                if (this->send(&lastResponses[i])) {
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
                    display.setBrightness(i == nextRequest.whosTurn() ? 0xFF : 0xFF/10);
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

