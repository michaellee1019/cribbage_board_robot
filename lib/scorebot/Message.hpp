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
    TimestampT timestamp{0};
    PlayerNumberT whosTurn{0};
    TurnNumberT turnNumber{0};

    explicit operator bool() const {
        return turnNumber > 0;
    }

    void log(const char* name) const {
        print(timestamp);
        print("  Sends");
        print(name);
        print(">  ");

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

class WhatPlayerBoardAcksInResponse {
public:
    PlayerNumberT myPlayerNumber{-1};
    TurnNumberT iThinkItsNowTurnNumber{-1};
    ScoreT scoreDelta{0};
    bool commit{false};

    explicit operator bool() const {
        return myPlayerNumber >= 0;
    }

    //    void advanceForTesting() {
    //        scoreDelta += 10;
    //        iThinkItsNowTurnNumber++;
    //    }

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
        print(scoreDelta);

        print("\n");
    }
};

//enum class SendStatus { Sent, SentNoAck, Failure };
//const char* toString(const SendStatus& status) {
//    switch (status) {
//        case SendStatus::Sent:
//            return "Sent";
//        case SendStatus::SentNoAck:
//            return "SentNoAck";
//        case SendStatus::Failure:
//            return "Failure";
//    }
//    return "Unknown";
//}
//enum class ReceiveStatus {
//    Received,
//    ReceivedNoAckSent,
//    Failure,
//};
//const char* toString(const ReceiveStatus& status) {
//    switch (status) {
//        case ReceiveStatus::ReceivedNoAckSent:
//            return "ReceivedNoAckSent";
//        case ReceiveStatus::Received:
//            return "Received";
//        case ReceiveStatus::Failure:
//            return "Failure";
//    }
//    return "Unknown";
//}

#endif  // MESSAGE_HPP
