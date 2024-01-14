#include <Arduino.h>
#include <TM1637Display.h>

#include <scorebot/Devices.hpp>
#include <scorebot/PlayerBoard.hpp>
#include <scorebot/ScoreBoard.hpp>

// <TM1637>
    #define PIN_CLK   8
    #define PIN_DIO   7
// </TM1637>

// <Turn LED>
    #define PIN_TURN_LED  2
// </Turn LED>

// <Buttons>
    // Plus 5
    #define PIN_BUTTON_0  3
    // Plus 1
    #define PIN_BUTTON_1  4
    // Minus 1
    #define PIN_BUTTON_2  5
    // Commit
    #define PIN_BUTTON_3  6
// </Buttons>


// <DIP Switches>
    #define PIN_DIP_0   14
    #define PIN_DIP_1   15
    #define PIN_DIP_2   16
    #define PIN_DIP_3   17
// </DIP Switches>

static TM1637Display display(PIN_CLK, PIN_DIO);


IODevice* self;
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
       PIN_BUTTON_3
    });

    pinMode(PIN_DIP_0, INPUT);
    pinMode(PIN_DIP_1, INPUT);
    pinMode(PIN_DIP_2, INPUT);
    pinMode(PIN_DIP_3, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);

    display.setBrightness(0x0f);

    pinMode(PIN_TURN_LED, OUTPUT);

    for(int i=0; i<3; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }
}

struct Message {
    Message()
    : senderScore{-1}, receiverScore{-1}, turnNumber{-1}
    {}

    explicit operator bool() const {
        return turnNumber >= 0;
    }

    // senderScore is the current total score for the player that score board knows about
    int senderScore;

    // receiverScore is the new score that the player knows about
    int receiverScore;
    int turnNumber;

    void log(const char* name) const {
        if (!*this) {
            return;
        }
        Serial.print("<");
        Serial.print(name);
        Serial.print(">");
        this->sendToSerial();
        Serial.print("</");
        Serial.print(name);
        Serial.print(">");
        Serial.print("\n");
    }
    private:
    void sendToSerial() const {
        Serial.print("sender=");
        Serial.print(this->senderScore);
        Serial.print(" rcvr=");
        Serial.print(this->receiverScore);
        Serial.print(" turn=");
        Serial.print(this->turnNumber);
    }
};
Message message;

void loop() {
    self->loop();
}

