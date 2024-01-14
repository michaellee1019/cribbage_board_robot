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
} txState;


//===============

void setup() {

    Serial.begin(9600);
    Serial.println(F("Source File /mnt/sdb1/SGT-Prog/Arduino/ForumDemos/nRF24Tutorial/SimpleTxAckPayload.ino"));
    Serial.println("SimpleTxAckPayload Starting");

    txState.radio.begin();
//    txState.radio.setPALevel(RF24_PA_MAX);
    txState.radio.setDataRate( RF24_250KBPS );

    txState.radio.enableAckPayload();

    txState.radio.setRetries(5,5); // delay, count
                                       // 5 gives a 1500 µsec delay which is needed for a 32 byte ackPayload
    txState.radio.openWritingPipe(txState.slaveAddress);
}

//=============

void send();
void showData();
void updateMessage();

void loop() {

    txState.currentMillis = millis();
    if (txState.currentMillis - txState.prevMillis >= txState.txIntervalMillis) {
        send();
    }
    showData();
}

//================

void send() {

    bool rslt;
    rslt = txState.radio.write( &txState.dataToSend, sizeof(txState.dataToSend) );
        // Always use sizeof() as it gives the size as the number of bytes.
        // For example if dataToSend was an int sizeof() would correctly return 2

    Serial.print("Data Sent ");
    Serial.print(txState.dataToSend);
    if (rslt) {
        if ( txState.radio.isAckPayloadAvailable() ) {
            txState.radio.read(&txState.ackData, sizeof(txState.ackData));
            txState.newData = true;
        }
        else {
            Serial.println("  Acknowledge but no data ");
        }
        updateMessage();
    }
    else {
        Serial.println("  Tx failed");
    }

    txState.prevMillis = millis();
 }


//=================

void showData() {
    if (txState.newData == true) {
        Serial.print("  Acknowledge data ");
        Serial.print(txState.ackData[0]);
        Serial.print(", ");
        Serial.println(txState.ackData[1]);
        Serial.println();
        txState.newData = false;
    }
}

//================

void updateMessage() {
        // so you can see that new data is being sent
    txState.txNum += 1;
    if (txState.txNum > '9') {
        txState.txNum = '0';
    }
    txState.dataToSend[8] = txState.txNum;
}

} // namespace tx

#endif

