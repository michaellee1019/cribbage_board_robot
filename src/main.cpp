#include <Arduino.h>

#include "printf.h"
#include <scorebot/Devices.hpp>
#include <scorebot/LeaderBoard.hpp>
#include <scorebot/PlayerBoard.hpp>

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
    IOConfig config{.pinButton0 = 3,
                    .pinButton1 = 4,
                    .pinButton2 = 5,
                    .pinButton3 = 6,
                    .pinDip0 = 14,
                    .pinDip1 = 15,
                    .pinDip2 = 16,
                    .pinDip3 = 17,
                    .pinLedBuiltin = LED_BUILTIN,
                    .pinTurnLed = 2};
    Serial.begin(9600);
    printf_begin();
    std::cout << "ScoreBotSetup BOARD_ID=" << BOARD_ID << std::endl;

    if (BOARD_ID == -1) {
        self = new LeaderBoard(config);
    } else {
        self = new PlayerBoard(config);
    }

    self->setup();
    blink();
}

void loop() {
    self->loop();
}
