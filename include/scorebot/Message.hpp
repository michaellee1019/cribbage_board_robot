#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Arduino.h>


class Message {
    int senderScore;
    int receiverScore;
    int turnNumber;

public:
    Message()
    : senderScore{-1},
      receiverScore{-1},
      turnNumber{-1}
    {}

    explicit operator bool() const {
        return turnNumber >= 0;
    }

    void nextTurn() {
        this->turnNumber = (this->turnNumber % 200) + 1;
    }

    void log(const char *name) const {
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

#endif
