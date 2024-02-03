#include "RF24.h"
#include "printf.h"
#include "TM1637Display.h"

#include <scorebot/Message.hpp>
#include <scorebot/Devices.hpp>
#include <scorebot/PlayerBoard.hpp>
#include <scorebot/ScoreBoard.hpp>

// Cruft
#define CE_PIN   10
#define CSN_PIN 9


struct ScoreboardState {
    const byte thisSlaveAddress[5] = {'R','x','A','A','A'};
    RF24 radio{CE_PIN, CSN_PIN};

    void setup() {
        radio.begin();
        radio.setDataRate( RF24_250KBPS );
        radio.openReadingPipe(1, thisSlaveAddress);

        radio.enableAckPayload();

        radio.startListening();

        WhatScoreboardSends ack{};
        radio.writeAckPayload(1, &ack, sizeof(ack)); // pre-load data

        radio.printPrettyDetails();
    }

    void checkForMessages(WhatPlayerBoardSends* received, WhatScoreboardSends* ack) {
        if ( ! radio.available() ) {
            return;
        }
        radio.read( received, sizeof(WhatPlayerBoardSends) );
        // TODO: The comment below makes no sense after all these refactorings.
        //       Did I do this right? What is "the next time"? It sounds ominous.
        radio.writeAckPayload(1, ack, sizeof(WhatScoreboardSends)); // load the payload for the next time
    }
    void loop() {
        WhatPlayerBoardSends received{};
        WhatScoreboardSends ack{};
        this->checkForMessages(&received, &ack);
        received.log("Received");
    }
} rxState;


struct PlayerBoardState {
    const byte slaveAddress[5] = {'R','x','A','A','A'};
    RF24 radio{CE_PIN, CSN_PIN}; // Create a Radio

    void setup() {
        radio.begin();
        radio.setDataRate( RF24_250KBPS );
        radio.enableAckPayload();
        radio.setRetries(5,5); // delay, count
        // TODO: 5 gives a 1500 µsec delay which is needed for a 32 byte ackPayload
        radio.openWritingPipe(slaveAddress);

        radio.printPrettyDetails();
    }

    unsigned long lastSent = 0;
    WhatPlayerBoardSends state{};

    void loop() {
        auto time = millis();
        if (time - lastSent >= 1000) {
            state.advanceForTesting();
            WhatScoreboardSends ack{};
            this->send(&state, &ack);
            state.log("Sent");
            lastSent = time;
        }
    }

    void send(WhatPlayerBoardSends* toSend, WhatScoreboardSends* ack) {
        bool rslt = this->radio.write( toSend, sizeof(WhatPlayerBoardSends) );
        if (rslt) {
            if ( radio.isAckPayloadAvailable() ) {
                radio.read(ack, sizeof(WhatScoreboardSends));
            }
        }
    }
} txState;
// </Cruft>


void scorebotSetup(const IOConfig& config) {
    Serial.begin(9600);
    printf_begin();
    std::cout << "ScoreBotSetup BOARD_ID=" << BOARD_ID << std::endl;

    pinMode(config.pinButton0, INPUT);
    pinMode(config.pinButton1, INPUT);
    pinMode(config.pinButton2, INPUT);
    pinMode(config.pinButton3, INPUT);

    digitalWrite(config.pinButton0, HIGH);
    digitalWrite(config.pinButton1, HIGH);
    digitalWrite(config.pinButton2, HIGH);
    digitalWrite(config.pinButton3, HIGH);

    pinMode(config.pinDip0, INPUT);
    pinMode(config.pinDip1, INPUT);
    pinMode(config.pinDip2, INPUT);
    pinMode(config.pinDip3, INPUT);

    pinMode(config.pinLedBuiltin, OUTPUT);
    pinMode(config.pinTurnLed, OUTPUT);
}


TabletopBoard::TabletopBoard() = default;



// PlayerBoard

struct PlayerBoard::Impl {
    TM1637Display displayP2;
    TM1637Display displayP3;
    TM1637Display displayP4;

    Impl()
            : displayP2(6, 5),
              displayP3(4, 3),
              displayP4(21, 2)
    {}

    void setup() {
        displayP2.setBrightness(0x0f);
        displayP2.showNumberDec(2);
        displayP3.setBrightness(0x0f);
        displayP3.showNumberDec(3);
        displayP4.setBrightness(0x0f);
        displayP4.showNumberDec(4);
    }

    void loop() {
        displayP2.setBrightness(0x0f);
        displayP2.showNumberDec(2);
        displayP3.setBrightness(0x0f);
        displayP3.showNumberDec(3);
        displayP4.setBrightness(0x0f);
        displayP4.showNumberDec(4);
    }
};

PlayerBoard::PlayerBoard()
: impl{new Impl()}
{}

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup(const IOConfig& config) {
    scorebotSetup(config);
    rxState.setup();
    impl->setup();
}

void PlayerBoard::loop() {
    rxState.loop();
    impl->loop();
}



// ScoreBoard

ScoreBoard::~ScoreBoard() = default;
void ScoreBoard::setup(const IOConfig& config) {
    scorebotSetup(config);
    txState.setup();
}

void ScoreBoard::loop() {
    txState.loop();
}

void blink() {
    for(int i=0; i<3; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }
}