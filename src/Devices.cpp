#include "scorebot/Devices.hpp"
#include "scorebot/PlayerBoard.hpp"
#include "scorebot/ScoreBoard.hpp"
#include "RF24.h"


// Cruft

#define CE_PIN   9
#define CSN_PIN 10

struct RxState {
    const byte thisSlaveAddress[5] = {'R','x','A','A','A'};
    RF24 radio{CE_PIN, CSN_PIN};
    char dataReceived[10]; // this must match dataToSend in the TX
    int ackData[2] = {109, -4000}; // the two values to be sent to the master
    bool newData = false;

    void setup() {
        radio.begin();
        radio.setDataRate( RF24_250KBPS );
        radio.openReadingPipe(1, thisSlaveAddress);

        radio.enableAckPayload();

        radio.startListening();

        radio.writeAckPayload(1, &ackData, sizeof(ackData)); // pre-load data
    }

    void getData() {
        if ( radio.available() ) {
            radio.read( &dataReceived, sizeof(dataReceived) );
            updateReplyData();
            newData = true;
        }
    }

    void updateReplyData() {
        ackData[0] -= 1;
        ackData[1] -= 1;
        if (ackData[0] < 100) {
            ackData[0] = 109;
        }
        if (ackData[1] < -4009) {
            ackData[1] = -4000;
        }
        radio.writeAckPayload(1, &ackData, sizeof(ackData)); // load the payload for the next time
    }

    void showData() {
        if (newData == true) {
            Serial.print("Data received ");
            Serial.println(dataReceived);
            Serial.print(" ackPayload sent ");
            Serial.print(ackData[0]);
            Serial.print(", ");
            Serial.println(ackData[1]);
            newData = false;
        }
    }

    void loop() {
        this->getData();
        this->showData();
    }
} rxState;


struct TxState {
    const byte slaveAddress[5] = {'R','x','A','A','A'};
    RF24 radio{CE_PIN, CSN_PIN}; // Create a Radio

    char dataToSend[10] = "Message 0";
    char txNum = '1';
    int ackData[2] = {-1, -1}; // to hold the two values coming from the slave
    bool newData = false;

    unsigned long currentMillis;
    unsigned long prevMillis;
    unsigned long txIntervalMillis = 1000; // send once per second

    void setup() {
        radio.begin();
        radio.setDataRate( RF24_250KBPS );

        radio.enableAckPayload();

        radio.setRetries(5,5); // delay, count
        // 5 gives a 1500 µsec delay which is needed for a 32 byte ackPayload
        radio.openWritingPipe(slaveAddress);
    }

    void loop() {
        this->currentMillis = millis();
        if (this->currentMillis - this->prevMillis >= this->txIntervalMillis) {
            send();
        }
        showData();
    }

    void showData() {
        if (this->newData == true) {
            Serial.print("  Acknowledge data ");
            Serial.print(this->ackData[0]);
            Serial.print(", ");
            Serial.println(this->ackData[1]);
            Serial.println();
            this->newData = false;
        }
    }

    void send() {
        bool rslt;
        rslt = this->radio.write( &this->dataToSend, sizeof(this->dataToSend) );
        // Always use sizeof() as it gives the size as the number of bytes.
        // For example if dataToSend was an int sizeof() would correctly return 2

        Serial.print("Data Sent ");
        Serial.print(this->dataToSend);
        if (rslt) {
            if ( this->radio.isAckPayloadAvailable() ) {
                this->radio.read(&this->ackData, sizeof(this->ackData));
                this->newData = true;
            }
            else {
                Serial.println("  Acknowledge but no data ");
            }
            updateMessage();
        }
        else {
            Serial.println("  Tx failed");
        }

        this->prevMillis = millis();
    }

    void updateMessage() {
        // so you can see that new data is being sent
        this->txNum += 1;
        if (this->txNum > '9') {
            this->txNum = '0';
        }
        this->dataToSend[8] = this->txNum;
    }
} txState;
// </Cruft>


IODevice::IODevice() = default;



// PlayerBoard

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
    rxState.setup();
}

void PlayerBoard::loop() {
    rxState.loop();
}



// ScoreBoard

ScoreBoard::~ScoreBoard() = default;
void ScoreBoard::setup() {
    txState.setup();
}

void ScoreBoard::loop() {
    txState.loop();
}


void scorebotSetup(IOConfig config) {
    Serial.begin(9600);
    std::cout << "hello!" << std::endl;

    pinMode(config.pinButton0, INPUT);
    pinMode(config.pinButton1, INPUT);
    pinMode(config.pinButton2, INPUT);
    pinMode(config.pinButton3, INPUT);

    digitalWrite(config.pinButton0, HIGH);
    digitalWrite(config.pinButton1, HIGH);
    digitalWrite(config.pinButton2, HIGH);
    digitalWrite(config.pinButton3, HIGH);
}