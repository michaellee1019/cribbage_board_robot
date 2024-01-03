#include <Wire.h>
#include "TM1637Display.h"
#define CLK 9
#define DIO 10
TM1637Display display(CLK, DIO);

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, 6,
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB           + NEO_KHZ800);

enum class Color : uint32_t {
  red = 0xFF0000,
  yellow = 0xFFFF00,
  green = 0x00FF00,
  blue = 0x0000FF
};


class Player {       // The class
  public:             // Access specifier
    Color color;   // Attribute (int variable)
    int score;        // Attribute (string variable)
  Player(Color color)
  : color{color}, score{0} {};
};



int currPlayer = 0;
const int players = 4;
Player Players[players] = {{Color::red}, {Color::yellow}, {Color::green}, {Color::blue}};




void setup() {
  // note that begin() has no parameter.
  Wire.begin();
  Serial.begin(9600);
  display.setBrightness(0x0f);

  //led matrix
  matrix.begin();
  matrix.setBrightness(1);
  }

typedef struct {
  char state;
  short score;
}
Message;

class Scoreboard {
  public:
  
};

class Controller {
  
}

void loop() {
  display.showNumberDec(Players[currPlayer].score, false);
  int address = 8;
  Message currentMessage;
  int requestBytes = sizeof currentMessage;
  Wire.requestFrom(address, requestBytes);
  Wire.readBytes( (byte *) &currentMessage, sizeof(currentMessage));

  matrix.clear();
  matrix.fill(static_cast<uint32_t>(Players[currPlayer].color), 40, currentMessage.score);
  matrix.show();
  
//  Serial.print("message: ");
//  Serial.print(currentMessage.state);
//  Serial.print(" ");
//  Serial.println(currentMessage.score);
  if(currentMessage.state == 'c') {
    Players[currPlayer].score += currentMessage.score;
    for (byte i = 0 ; i<2;i++){
      delay(250);
      display.clear();
      delay(250);
      display.showNumberDec(Players[currPlayer].score, false);
    }
    if(currPlayer == players-1) {
      currPlayer = 0;
    } else {
      currPlayer++;
    }
  }
}
