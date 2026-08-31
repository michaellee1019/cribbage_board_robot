#ifndef EVENT_H
#define EVENT_H

#include <Protocol.hpp>

#include <cstdint>

enum class EventType {
    ButtonPressed,
    BleConnected,
    BleLeaderLost,
    NewPeer,
    MessageReceived,
    StateUpdate,
    LostPeer,
};

enum class ButtonName {
    GPIOButtons,
    RotaryEncoder,
};

struct ButtonPressEvent {
    // uint8_t buttonId;
    ButtonName buttonName;
};
struct BleConnectedEvent {};
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
struct StateUpdateEvent {};

struct Event {
    EventType type;
    union {
        ButtonPressEvent press;
        BleConnectedEvent bleConnected;
        NewPeerEvent newPeer;
        MessageReceivedEvent messageReceived;
        StateUpdateEvent state;
        LostPeerEvent lostPeer;
    };
};

#endif // EVENT_H
