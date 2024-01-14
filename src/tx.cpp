// SimpleTxAckPayload - the master or the transmitter
#ifndef TX_CPP
#define TX_CPP

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"


#define CE_PIN   9
#define CSN_PIN 10

namespace tx {


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


//===============

void setup() {
    Serial.begin(9600);
    txState.setup();
}
void loop() {
    txState.loop();
}


}

#endif

