#ifndef EVENT_H
#define EVENT_H

#include <Protocol.hpp>

#include <cstdint>

enum class EventType {
    ButtonPressed,
    BleLeaderLost,
    NewPeer,
    MessageReceived,
    LostPeer,
};

enum class ButtonName {
    GPIOButtons,
    RotaryEncoder,
};

struct ButtonPressEvent {
    ButtonName buttonName;
};
struct LostPeerEvent {
    uint32_t peerId;
};
struct NewPeerEvent {
    uint32_t peerId;
};
struct MessageReceivedEvent {
    uint32_t peerId;
    uint16_t connectionHandle;
    char message[scorebot::kMaxWireMessageSize + 1];
};
struct Event {
    EventType type;
    union {
        ButtonPressEvent press;
        NewPeerEvent newPeer;
        MessageReceivedEvent messageReceived;
        LostPeerEvent lostPeer;
    };
};

#endif // EVENT_H
