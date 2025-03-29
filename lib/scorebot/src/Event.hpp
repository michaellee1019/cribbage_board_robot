#ifndef EVENT_H
#define EVENT_H

#include <cstdint>

enum class EventType { ButtonPressed, WifiConnected };

struct Event {
    EventType type;
    union {
        uint8_t buttonId;
        char wifiMessage[256];
    };
};

#endif // EVENT_H
