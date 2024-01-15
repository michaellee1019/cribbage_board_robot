#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Arduino.h>

struct WhatScoreboardSends {
    int a {-1};
    int b {-1};


    void log(const char* name) const {
        Serial.print("<WhatScoreboardSends");
        Serial.print(name);
        Serial.print(" a=");
        Serial.print(this->a);
        Serial.print(" b=");
        Serial.print(this->b);
        Serial.print(">");
        Serial.print("\n");
    }
};

class WhatPlayerBoardSends {
    int senderScore;
    int receiverScore;
    int turnNumber;

public:
    WhatPlayerBoardSends()
    : WhatPlayerBoardSends{-1, -1, -1}
    {}

    WhatPlayerBoardSends(int senderScore, int receiverScore, int turnNumber)
    : senderScore{senderScore},
      receiverScore{receiverScore},
      turnNumber{turnNumber}
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

enum class SendStatus {
    Sent,
    SentNoAck,
    Failure
};
const char* toString(const SendStatus& status) {
    switch(status) {
        case SendStatus::Sent:      return "Sent";
        case SendStatus::SentNoAck: return "SentNoAck";
        case SendStatus::Failure:   return "Failure";
    }
    return "Unknown";
}
enum class ReceiveStatus {
    Received,
    ReceivedNoAckSent,
    Failure,
};
const char* toString(const ReceiveStatus& status) {
    switch(status) {
        case ReceiveStatus::ReceivedNoAckSent: return "ReceivedNoAckSent";
        case ReceiveStatus::Received:          return "Received";
        case ReceiveStatus::Failure:           return "Failure";
    }
    return "Unknown";
}

#endif
