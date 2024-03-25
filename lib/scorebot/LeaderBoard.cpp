#include "LeaderBoard.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"

#include "Adafruit_FRAM_I2C.h"
#include "RF24.h"
#include "TM1637Display.h"

struct LeaderBoard::Impl {
    RF24 radio;
    const IOConfig& config;
    StateRefreshRequest nextRequest;
    StateRefreshResponse lastResponses[N_PLAYERS];
    Adafruit_FRAM_I2C i2ceeprom;

    explicit Impl(const IOConfig& config, TimestampT startupGeneration)
    : radio{config.pinRadioCE, config.pinRadioCSN},
      config{config},
      nextRequest{startupGeneration} {}

    TM1637Display displays[N_DISPLAYS] {
        // TODO: Put these pin numbers into IOConfig.
        TM1637Display(6, 5),
        TM1637Display(8, 7),
        TM1637Display(4, 3),
    };


    template<typename F>
    void eachDisplay(F&& callback) {
        for (size_t i = 0; i < N_DISPLAYS; ++i) {
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
        i2ceeprom.begin(0x50);
        i2ceeprom.readObject(0x00, nextRequest);

        this->displayStartup();

        doRadioSetup(radio);
        radio.openWritingPipe(reinterpret_cast<const uint8_t*>("RxAAA"));
        radio.printPrettyDetails();

        eachDisplay([&](TM1637Display& display, int i) {
            display.showNumberDec(nextRequest.state.scores[i]);
        });
    }



    bool send(StateRefreshResponse* responseReceived) {
        return doSend(&this->radio, &nextRequest, [&]() { return doRead(&this->radio, responseReceived); });
    }

    const byte slaveAddresses[N_PLAYERS][5] = {
        {'R', 'x', 'A', 'A', 'A'},
        {'R', 'x', 'A', 'A', 'B'},
        {'R', 'x', 'A', 'A', 'C'},
    };

    Periodically everySecond{500};
    void loop() {  // Leaderboard

        everySecond.run(millis(), [&]() {
            for (PlayerNumberT i=0; i < N_PLAYERS; ++i) {
                this->radio.stopListening();
                this->radio.openWritingPipe(slaveAddresses[i]);
                this->send(&lastResponses[i]);
            }
        });

        this->nextRequest.update(lastResponses);
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

