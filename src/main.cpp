#include <Arduino.h>
#include <ArduinoSTL.h>

#include "printf.h"
#include "TabletopBoard.hpp"
#include "LeaderBoard.hpp"
#include "PlayerBoard.hpp"

void blink() {
    for (int i = 0; i < 3; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }
}

TabletopBoard* self;

void setup() {
    constexpr IOConfig config {
        .pinCommit = 2,
        .pinNegOne = 5,
        .pinPlusFive = 4,
        .pinPlusOne = 3,
        .pinPassTurn = 6,
        .pinDip0 = 14,
        .pinDip1 = 15,
        .pinDip2 = 16,
        .pinDip3 = 17,
        .pinLedBuiltin = LED_BUILTIN,
        .pinTurnLed = 21,
        .pinRadioCE = 10,
        .pinRadioCSN = 9
    };
    Serial.begin(9600);
    printf_begin();
    std::cout << "ScoreBotSetup BOARD_ID=" << BOARD_ID << std::endl;

    TimestampT startupGeneration = millis();

    if constexpr (BOARD_ID == -1) {
        self = new LeaderBoard(config, startupGeneration);
    } else {
        self = new PlayerBoard(config, startupGeneration);
    }

    self->setup();
    blink();
}

void loop() {
    self->loop();
}
