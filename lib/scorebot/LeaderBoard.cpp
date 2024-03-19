#include "LeaderBoard.hpp"
#include "Message.hpp"
#include "RadioHelper.hpp"
#include "Utility.hpp"

#include "RF24.h"
#include "TM1637Display.h"

struct LeaderBoard::Impl {
    RF24 radio;
    const IOConfig& config;
    StateRefreshRequest request;
    StateRefreshResponse responses[N_PLAYERS];

    explicit Impl(const IOConfig& config, TimestampT startupGeneration)
    : radio{config.pinRadioCE, config.pinRadioCSN},
      config{config},
      request{StateRefreshResponse.startup()} {

    }

    TM1637Display displays[N_DISPLAYS] {
        // TODO: Put these pin numbers into IOConfig.
        //TM1637Display(8, 7),
        TM1637Display(6, 5),
        TM1637Display(4, 3)
        //TM1637Display(2, 21)
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



    bool send(StateRefreshResponse* responseReceived) {
        return doSend(&this->radio, &request, [&]() { return doRead(&this->radio, responseReceived); });
    }

    const byte slaveAddresses[N_PLAYERS][5] = {
        {'R', 'x', 'A', 'A', 'A'},
        {'R', 'x', 'A', 'A', 'B'}
    };

    Periodically second{100};
    void loop() {  // Leaderboard

        second.run(millis(), [&]() {
            for (PlayerNumberT i=0; i < N_PLAYERS; ++i) {
                this->radio.stopListening();
                this->radio.openWritingPipe(slaveAddresses[i]);
                this->send(&responses[i]);
            }
        });

        this->request.update(responses);
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

