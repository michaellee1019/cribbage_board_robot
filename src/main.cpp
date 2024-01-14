#include <Arduino.h>

#include <scorebot/Pins.hpp>

#include <scorebot/Devices.hpp>
#include <scorebot/PlayerBoard.hpp>
#include <scorebot/ScoreBoard.hpp>

IODevice* self;

void blink() {
    for(int i=0; i<3; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }
}

void setup() {
    if (BOARD_ID == -1) {
        self = new ScoreBoard();
    } else {
        self = new PlayerBoard();
    }
    self->setup();

    scorebotSetup({
       PIN_BUTTON_0,
       PIN_BUTTON_1,
       PIN_BUTTON_2,
       PIN_BUTTON_3,
       PIN_DIP_0,
       PIN_DIP_1,
       PIN_DIP_2,
       PIN_DIP_3,
       LED_BUILTIN,
       PIN_TURN_LED,
    });
    blink();
}

void loop() {
    self->loop();
}

