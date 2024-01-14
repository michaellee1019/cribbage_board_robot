#include "RF24.h"
#include "printf.h"

#include <scorebot/Message.hpp>
#include <scorebot/Devices.hpp>
#include <scorebot/PlayerBoard.hpp>
#include <scorebot/ScoreBoard.hpp>

// Cruft
#define CE_PIN   9
#define CSN_PIN 10

//struct MyRadio {
//    RF24 _radio;
//    explicit MyRadio(int cePin, int csnPin)
//    : _radio{cePin, csnPin} {}
//};


struct RxState {
    const byte thisSlaveAddress[5] = {'R','x','A','A','A'};
    RF24 radio{CE_PIN, CSN_PIN};

    void setup() {
        radio.begin();
        radio.setDataRate( RF24_250KBPS );
        radio.openReadingPipe(1, thisSlaveAddress);

        radio.enableAckPayload();

        radio.startListening();

        Ack ack;
        radio.writeAckPayload(1, &ack, sizeof(ack)); // pre-load data

        radio.printPrettyDetails();
    }

    void getData(Message* received, Ack* ack) {
        if ( radio.available() ) {
            radio.read( received, sizeof(Message) );
            radio.writeAckPayload(1, ack, sizeof(Ack)); // load the payload for the next time
        }
    }
    void loop() {
        Message received;
        Ack ack;
        this->getData(&received, &ack);
        received.log("Received");
    }
} rxState;


struct TxState {
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
    Message state;

    void loop() {
        auto time = millis();
        if (time - lastSent >= 1000) {
            state.nextTurn();
            Ack ack;
            this->send(&state, &ack);
            state.log("Sent");
            lastSent = time;
        }
    }

    void send(Message* toSend, Ack* ack) {
        bool rslt = this->radio.write( toSend, sizeof(Message) );
        if (rslt) {
            if ( radio.isAckPayloadAvailable() ) {
                radio.read(ack, sizeof(Ack));
            }
        }
    }
} txState;
// </Cruft>


void scorebotSetup(const IOConfig& config) {
    Serial.begin(9600);
    printf_begin();
    std::cout << "hello!" << std::endl;

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

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup(const IOConfig& config) {
    scorebotSetup(config);
    rxState.setup();
}

void PlayerBoard::loop() {
    rxState.loop();
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