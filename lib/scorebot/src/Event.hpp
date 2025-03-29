#ifndef EVENT_H
#define EVENT_H

#include <cstdint>

enum class EventType { ButtonPressed, WifiConnected, NewPeer, MessageReceived, StateUpdate };

struct ButtonPressEvent {
    uint8_t buttonId;
};
struct WifiConnectedEvent {};
struct NewPeerEvent {
    uint8_t peerId;
};
struct MessageReceivedEvent {
    uint8_t peerId;
    char wifiMessage[256];
};
struct StateUpdateEvent {};

struct Event {
    EventType type;
    union {
        ButtonPressEvent press;
        WifiConnectedEvent wifiConnected;
        NewPeerEvent newPeer;
        MessageReceivedEvent messageReceived;
        StateUpdateEvent state;
    };
};

// struct EventVisitor {
//     virtual void onButtonPress(ButtonPressEvent event) {}
//     virtual void onWifiConnected(WifiConnectedEvent event) {}
//     virtual void onNewPeer(NewPeerEvent event) {}
//     virtual void onMessageReceived(MessageReceivedEvent event) {}
//     virtual void onStateUpdate(StateUpdateEvent event) {}
//     virtual ~EventVisitor() = default;
//     void onEvent(const Event& event) {
//         switch (event.type) {
//             case EventType::ButtonPressed: this->onButtonPress(event.press); break;
//             case EventType::WifiConnected: this->onWifiConnected(event.wifiConnected); break;
//             case EventType::NewPeer: this->onNewPeer(event.newPeer); break;
//             case EventType::MessageReceived: this->onMessageReceived(event.messageReceived); break;
//             case EventType::StateUpdate: this->onStateUpdate(event.state); break;
//         }
//     }
// };

#endif // EVENT_H
