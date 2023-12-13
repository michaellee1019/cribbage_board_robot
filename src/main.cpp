#include <Arduino.h>
#include <RadioLib.h>

#if 0
Random Notes
============

    Board = Master <-> Sender
    P1 = Slave <-> Receiver RxAAA
    P2 = Slave <-> Receiver RxBBB
    P3 = Slave <-> Receiver RxCCC
    P4 = Slave <-> Receiver RxDDD

#endif

// <RF Module>
  // Chip Enable
  #define PIN_CE      9
  // Chip Select Not
  #define PIN_CSN     10
// </RF Module>

// <TM1637>
    #define PIN_CLK   8
    #define PIN_DIO   7
// </TM1637>

// <Turn LED>
    #define PIN_TURN_LED  2
// </Turn LED>

// <Buttons>
    // Plus 5
    #define PIN_BUTTON_0  3
    // Plus 1
    #define PIN_BUTTON_1  4
    // Minus 1
    #define PIN_BUTTON_2  5
    // Commit
    #define PIN_BUTTON_1  6
// </Buttons>


// <DIP Switches>
    #define PIN_DIP_0   14
    #define PIN_DIP_1   15
    #define PIN_DIP_2   16
    #define PIN_DIP_3   17
// </DIP Switches>


void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    // TODO: this isn't working :(
    Serial.println("Completed Iteration");
}
