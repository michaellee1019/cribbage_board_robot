#include <Arduino.h>
#include <ArduinoSTL.h>
#include "printf.h"

#include "BoardTypes.hpp"

TabletopBoard* self;

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
}


 void loop() {
     self->loop();
 }