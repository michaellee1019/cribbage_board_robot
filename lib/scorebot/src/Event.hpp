#ifndef EVENT_H
#define EVENT_H

#include <cstdint>

enum class EventType { ButtonPressed, WifiConnected, NewPeer, MessageReceived };

struct Event {
    EventType type;
    union {
        uint8_t buttonId;
        uint8_t peerId;
        char wifiMessage[256];
    };
};

#endif // EVENT_H
