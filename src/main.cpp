#include <Arduino.h>
#include <ArduinoSTL.h>
#include "printf.h"
#include <Wire.h>

#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


#include "BoardTypes.hpp"

TabletopBoard* self;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    constexpr IOConfig config{.pinCommit = 2,
                              .pinNegOne = 5,
                              .pinPlusFive = 4,
                              .pinPlusOne = 3,
                              .pinPassTurn = 6,
                              .pinLedBuiltin = LED_BUILTIN,
                              .pinTurnLed = 21,
                              .pinRadioCE = 10,
                              .pinRadioCSN = 9};
    Serial.begin(9600);
    printf_begin();
    std::cout << "ScoreBotSetup BOARD_ID=" << BOARD_ID << std::endl;

    const TimestampT startupGeneration = millis();

    if constexpr (BOARD_ID == -1) {
        self = new LeaderBoard(config, startupGeneration);
    } else {
        self = new PlayerBoard(config, startupGeneration);
    }

    self->setup();

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
}

int i = 0;
void updateAnimation(Adafruit_SSD1306& disp) {
    display.clearDisplay();
    display.setTextSize(2);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(i/25 + i%25, i % 25);     // Start at top-left corner
    display.cp437(true);         // Use full 256 char 'Code Page 437' font

    display.print(i, 10);

    display.display();
    i++;
}


 void loop() {
     self->loop();
     updateAnimation(display);
 }