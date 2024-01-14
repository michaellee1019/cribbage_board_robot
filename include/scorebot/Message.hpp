#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Arduino.h>


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

#endif
