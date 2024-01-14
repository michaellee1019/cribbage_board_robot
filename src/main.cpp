#include <Arduino.h>

#include <scorebot/Devices.hpp>
#include <scorebot/PlayerBoard.hpp>
#include <scorebot/ScoreBoard.hpp>

TabletopBoard* self;

void setup() {
    if (BOARD_ID == -1) {
        self = new ScoreBoard();
    } else {
        self = new PlayerBoard();
    }
    self->setup({
        .pinButton0    = 3,
        .pinButton1    = 4,
        .pinButton2    = 5,
        .pinButton3    = 6,
        .pinDip0       = 14,
        .pinDip1       = 15,
        .pinDip2       = 16,
        .pinDip3       = 17,
        .pinLedBuiltin = LED_BUILTIN,
        .pinTurnLed    = 2
    });

    blink();
}

void loop() {
    self->loop();
}

