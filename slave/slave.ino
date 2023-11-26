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
#define CLK 9
#define DIO 10
TM1637Display display(CLK, DIO);

// i2c settings
#include <Wire.h>

byte fivePin = 2;
byte onePin = 3;
byte neg1Pin = 4;
byte okPin = 5;
short score = 0; //todo change to short
short commitScore = 0; //todo change to short
// state means:
// "" = nothing
// t = turn went
// a = adjustment
char state = 'n';

byte prevFive = HIGH;
byte prevOne = HIGH;
byte prevNeg1 = HIGH;
byte prevOk = HIGH;

typedef struct {
  char state;
  short score;
}
Message;

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

  Wire.begin(8); // join the I2C bus with address 8
  Wire.onRequest(requestEvent);
  Serial.begin(9600); // start serial for output
}

void loop(){
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
      delay(250);
      display.clear();
      delay(250);
      display.showNumberDec(score, false);
    }
    state = 'c';
    commitScore = score;
    score = 0;
  }

  display.showNumberDec(score, false);
  delay(100);
}

void requestEvent(int bytes) {
  // if asking for state (one byte), return state
//  Serial.print("req bytes: ");
//  Serial.println(bytes);

  Message currentMessage;
  currentMessage.state = state;
  currentMessage.score = commitScore;

  Wire.write((byte *)&currentMessage, sizeof currentMessage);
  
//  if (bytes == 1) {
//    Serial.print("state: ");
//    Serial.println(state);
//    Wire.write(state);
//    return;
//  }
  //else return two byte short
//  Wire.write((byte *) &commitScore, sizeof(commitScore));
  Serial.print("message: ");
  Serial.print(currentMessage.state);
  Serial.print(" ");
  Serial.println(currentMessage.score);
  commitScore = 0;
  state='n';
}
