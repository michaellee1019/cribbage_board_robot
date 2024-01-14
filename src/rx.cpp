// SimpleRxAckPayload- the slave or the receiver
#ifndef RX_CPP
#define RX_CPP

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

namespace rx {

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
} rxState;



//==============

void setup() {
    Serial.begin(9600);
    Serial.println("SimpleRxAckPayload Starting");
    rxState.setup();
}

//==========

void getData();
void showData();
void updateReplyData();

void loop() {
    getData();
    showData();
}

//============

void getData() {
    if ( rxState.radio.available() ) {
        rxState.radio.read( &rxState.dataReceived, sizeof(rxState.dataReceived) );
        updateReplyData();
        rxState.newData = true;
    }
}

//================

void showData() {
    if (rxState.newData == true) {
        Serial.print("Data received ");
        Serial.println(rxState.dataReceived);
        Serial.print(" ackPayload sent ");
        Serial.print(rxState.ackData[0]);
        Serial.print(", ");
        Serial.println(rxState.ackData[1]);
        rxState.newData = false;
    }
}

//================

void updateReplyData() {
    rxState.ackData[0] -= 1;
    rxState.ackData[1] -= 1;
    if (rxState.ackData[0] < 100) {
        rxState.ackData[0] = 109;
    }
    if (rxState.ackData[1] < -4009) {
        rxState.ackData[1] = -4000;
    }
    rxState.radio.writeAckPayload(1, &rxState.ackData, sizeof(rxState.ackData)); // load the payload for the next time
}

} // namespace rx

#endif