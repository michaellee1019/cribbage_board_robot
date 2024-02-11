#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Arduino.h>

using PlayerNumberT = int;
using ScoreT = int;
using TurnNumberT = int;
using TimestampT = decltype(millis());

template <typename T>
void print(const T& t) {
    Serial.print(t);
}

struct WhatLeaderBoardSendsEverySecond {
    TimestampT timestamp;
    PlayerNumberT toPlayer;
    ScoreT yourScore;
    PlayerNumberT whosTurn;
    TurnNumberT turnNumber;

    void log(const char* name) const {
        print(timestamp);
        print("  Sends");
        print(name);
        print(">  ");

        print(" -");
        print(toPlayer);
        print(" #");
        print(yourScore);
        print(" ?");
        print(whosTurn);
        print(" @");
        print(turnNumber);

        print("\n");
    }
};

// I think this is a requirement of the NRF stack.
static_assert(sizeof(WhatLeaderBoardSendsEverySecond) <= 32,
              "WhatLeaderBoardSendsEverySecond max struct size");

class WhatPlayerBoardSends {
public:
    PlayerNumberT myPlayerNumber{-1};
    TurnNumberT iThinkItsNowTurnNumber{-1};
    ScoreT myScore{-1};

    explicit operator bool() const {
        return iThinkItsNowTurnNumber >= 0;
    }

    void advanceForTesting() {
        myScore += 10;
        iThinkItsNowTurnNumber++;
    }

    void log(const char* name) const {
        if (!*this) {
            return;
        }

        print("  PlayerSends");
        print(name);
        print(">  ");

        print(" N");
        print(myPlayerNumber);
        print(" T");
        print(iThinkItsNowTurnNumber);
        print(" S");
        print(myScore);

        print("\n");
    }
};

enum class SendStatus { Sent, SentNoAck, Failure };
const char* toString(const SendStatus& status) {
    switch (status) {
        case SendStatus::Sent:
            return "Sent";
        case SendStatus::SentNoAck:
            return "SentNoAck";
        case SendStatus::Failure:
            return "Failure";
    }
    return "Unknown";
}
enum class ReceiveStatus {
    Received,
    ReceivedNoAckSent,
    Failure,
};
const char* toString(const ReceiveStatus& status) {
    switch (status) {
        case ReceiveStatus::ReceivedNoAckSent:
            return "ReceivedNoAckSent";
        case ReceiveStatus::Received:
            return "Received";
        case ReceiveStatus::Failure:
            return "Failure";
    }
    return "Unknown";
}

#endif
