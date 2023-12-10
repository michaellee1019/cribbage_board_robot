// MultiTxAckPayload - the master or the transmitter
//   works with two Arduinos as slaves
//     each slave should the SimpleRxAckPayload program
//       one with the adress {'R','x','A','A','A'}
//         and the other with {'R','x','A','A','B'}

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>


#define CE_PIN   9
#define CSN_PIN 10

const byte numSlaves = 2;
const byte slaveAddress[numSlaves][5] = {
        // each slave needs a different address
                            {'R','x','A','A','A'},
                            {'R','x','A','A','B'}
                        };

RF24 radio(CE_PIN, CSN_PIN); // Create a Radio

//~ char dataToSend[10] = "Message 0";
char dataToSend[10] = "ToSlvN  0";
char txNum = '0';
int ackData[2] = {-1, -1}; // to hold the two values coming from the slave
bool newData = false;

unsigned long currentMillis;
unsigned long prevMillis;
unsigned long txIntervalMillis = 10; // send once per second

//===============

void setup() {

    Serial.begin(9600);
    Serial.println(F("Source File = /mnt/SGT/SGT-Prog/Arduino/ForumDemos/nRF24Tutorial/MultiTxAckPayload.ino "));
    Serial.println("SimpleTxAckPayload Starting");

    radio.begin();
    radio.setDataRate( RF24_250KBPS );

    radio.enableAckPayload();

    radio.setRetries(3,5); // delay, count
        // radio.openWritingPipe(slaveAddress); -- moved to loop()
}

//=============

void loop() {

    currentMillis = millis();
    if (currentMillis - prevMillis >= txIntervalMillis) {
        send();
    }
        // showData(); -- moved into send()
}

//================

void send() {

        // call each slave in turn
    for (byte n = 0; n < numSlaves; n++){

            // open the writing pipe with the address of a slave
        radio.stopListening();
        radio.openWritingPipe(slaveAddress[n]);

            // include the slave number in the message
        dataToSend[5] = n + '0';

        bool rslt;
        rslt = radio.write( &dataToSend, sizeof(dataToSend) );
            // Always use sizeof() as it gives the size as the number of bytes.
            // For example if dataToSend was an int sizeof() would correctly return 2

        Serial.print("  ========  For Slave ");
        Serial.print(n);
        Serial.println("  ========");
        Serial.print("  Data Sent ");
        Serial.print(dataToSend);
        if (rslt) {
            if ( radio.isAckPayloadAvailable() ) {
                radio.read(&ackData, sizeof(ackData));
                newData = true;
            }
            else {
                Serial.println("  Acknowledge but no data ");
            }
            updateMessage();
        }
        else {
            Serial.println("  Tx failed");
        }
        showData();
        Serial.print("\n");
    }

    prevMillis = millis();
 }


//=================

void showData() {
    if (newData == true) {
        Serial.print("  Acknowledge data ");
        Serial.print(ackData[0]);
        Serial.print(", ");
        Serial.println(ackData[1]);
        Serial.println();
        newData = false;
    }
}

//================

void updateMessage() {
        // so you can see that new data is being sent
    txNum += 1;
    if (txNum > '9') {
        txNum = '0';
    }
    dataToSend[8] = txNum;
}
