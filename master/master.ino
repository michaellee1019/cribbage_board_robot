
#include "TM1637Display.h"
//TM1637Display displayP1(8, 7);
TM1637Display displayP2(6, 5);
TM1637Display displayP3(4, 3);
TM1637Display displayP4(2, 21);

void setup() {
  // note that begin() has no parameter.
//  Serial.begin(9600);
//  displayP1.setBrightness(0x0f);
//  displayP1.showNumberDec(1);
  displayP2.setBrightness(0x0f);
  displayP2.showNumberDec(2);
  displayP3.setBrightness(0x0f);
  displayP3.showNumberDec(3);
  displayP4.setBrightness(0x0f);
  displayP4.showNumberDec(4);
}

void loop() {
//  displayP1.setBrightness(0x0f);
//  displayP1.showNumberDec(1);
  displayP2.setBrightness(0x0f);
  displayP2.showNumberDec(2);
  displayP3.setBrightness(0x0f);
  displayP3.showNumberDec(3);
  displayP4.setBrightness(0x0f);
  displayP4.showNumberDec(4);
}
