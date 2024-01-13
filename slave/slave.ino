/*
  Button

  Turns on and off a light emitting diode(LED) connected to digital pin 13,
  when pressing a pushbutton attached to pin 2.

  The circuit:
  - LED attached from pin 13 to ground through 220 ohm resistor
  - pushbutton attached to pin 2 from +5V
  - 10K resistor attached to pin 2 from ground

  - Note: on most Arduinos there is already an LED on the board
    attached to pin 13.

  created 2005
  by DojoDave <http://www.0j0.org>
  modified 30 Aug 2011
  by Tom Igoe

  This example code is in the public domain.

  https://www.arduino.cc/en/Tutorial/BuiltInExamples/Button
*/

#include "TM1637Display.h"
#define CLK 8
#define DIO 7
TM1637Display display(CLK, DIO);
uint8_t fullDisplay[] = { 0xff, 0xff, 0xff, 0xff };
uint8_t blankDisplay[] = { 0x00, 0x00, 0x00, 0x00 };

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN   9
#define CSN_PIN 10

const byte thisSlaveAddress[5] = {'R','x','A','A','A'};

RF24 radio(CE_PIN, CSN_PIN);

// i2c settings
//#include <Wire.h>

byte fivePin = 6;
byte onePin = 5;
byte neg1Pin = 4;
byte okPin = 3;
short score = 0;
// state means:
// n = nothing
// t = turn went
// a = adjustment
char state = 'n';

byte prevFive = HIGH;
byte prevOne = HIGH;
byte prevNeg1 = HIGH;
byte prevOk = HIGH;

typedef struct {
  bool turn;
}
InMessage;
InMessage inMessage = {turn:false};

typedef struct {
  char state;
  short score;
}
OutMessage;
OutMessage outMessage = {state:"n", score:0};

void setup() {
  pinMode(fivePin, INPUT);
  pinMode(onePin, INPUT);
  pinMode(neg1Pin, INPUT);
  pinMode(okPin, INPUT);

  //pull up resistor
  digitalWrite(fivePin, HIGH);
  digitalWrite(onePin, HIGH); 
  digitalWrite(neg1Pin, HIGH);
  digitalWrite(okPin, HIGH); 
  display.setBrightness(0x0f);
  display.setSegments(fullDisplay);
  delay(500);

  //dipSwitch
  pinMode(14, INPUT);
  pinMode(15, INPUT);
  pinMode(16, INPUT);
  pinMode(17, INPUT);

  //pull up resistor
  digitalWrite(14, HIGH);
  digitalWrite(15, HIGH); 
  digitalWrite(16, HIGH);
  digitalWrite(17, HIGH); 

  uint8_t zeroDigit = display.encodeDigit(0);
  uint8_t oneDigit = display.encodeDigit(1);
  uint8_t dipDisplayBits[] = { zeroDigit, zeroDigit, zeroDigit, zeroDigit };
  if(digitalRead(14) == LOW) {
    dipDisplayBits[0]=oneDigit;
  }
  if(digitalRead(15) == LOW) {
    dipDisplayBits[1]=oneDigit;
  }
  if(digitalRead(16) == LOW) {
    dipDisplayBits[2]=oneDigit;
  }
  if(digitalRead(17) == LOW) {
    dipDisplayBits[3]=oneDigit;
  } 

  display.setSegments(dipDisplayBits);
  delay(500);

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); 
  delay(500);
  digitalWrite(2, LOW); 

  
  Serial.begin(9600); // start serial for output

  radio.begin();
  radio.setDataRate( RF24_250KBPS );
  radio.openReadingPipe(1, thisSlaveAddress);

  radio.enableAckPayload();
  
  radio.startListening();
  radio.writeAckPayload(1, &outMessage, sizeof(outMessage)); // pre-load data
}

void loop(){
  getData();
  
  // 5pt
  byte fiveState = digitalRead(fivePin);
  if (fiveState == LOW && fiveState != prevFive) {
    score+=5; 
  }
  prevFive = fiveState; 

  // 1pt
  byte oneState = digitalRead(onePin);
  if (oneState == LOW && oneState != prevOne) {
    score++; 
  }
  prevOne = oneState;
  
  // -1pt
  byte neg1State = digitalRead(neg1Pin);
  if (neg1State == LOW && neg1State != prevNeg1) {
    score-=1; 
  }
  prevNeg1 = neg1State; 
  
  // commit
  if (digitalRead(okPin) == LOW)  
  {
    for (byte i = 0 ; i<2;i++){
      delay(100);
      display.clear();
      delay(100);
      display.showNumberDec(score, false);
    }
    if (inMessage.turn) {
      outMessage.state = 't';
    } else {
      outMessage.state = 'a';
    }
    outMessage.score = score;
    score = 0;
  }

  display.showNumberDec(score, false);
  updateReplyData();
  resetReplyData();

  delay(100);
}

void getData() {
    if ( radio.available() ) {
        radio.read( &inMessage, sizeof(inMessage) );
        //updateReplyData();
    }
}

void updateReplyData() {
  Serial.print("message: ");
  Serial.print(outMessage.state);
  Serial.print(" ");
  Serial.println(outMessage.score);
    radio.writeAckPayload(1, &outMessage, sizeof(outMessage)); // load the payload for the next time
}

void resetReplyData() {
    outMessage.state ='n';
    outMessage.score=0;
}

//void requestEvent(int bytes) {
//  // if asking for state (one byte), return state
////  Serial.print("req bytes: ");
////  Serial.println(bytes);
//
//  Message currentMessage;
//  currentMessage.state = state;
//  currentMessage.score = commitScore;
//
//  Wire.write((byte *)&currentMessage, sizeof currentMessage);
//  
////  if (bytes == 1) {
////    Serial.print("state: ");
////    Serial.println(state);
////    Wire.write(state);
////    return;
////  }
//  //else return two byte short
////  Wire.write((byte *) &commitScore, sizeof(commitScore));
//  Serial.print("message: ");
//  Serial.print(currentMessage.state);
//  Serial.print(" ");
//  Serial.println(currentMessage.score);
//  commitScore = 0;
//  state='n';
//}
