#include <Coordinator.hpp>
#include <GameState.hpp>

GameState::GameState() : score{}, whosTurn{0} {}

uint32_t otherPeer(Coordinator* coordinator, GameState* state) {
    for (const auto& peer : state->peers) {
        if (peer != coordinator->wifi.getMyPeerId()) {
            return peer;
        }
    }
    return 0;
}

// TODO: rename to onButtonChange
void onButtonPress(GameState* state, const ButtonPressEvent& e, Coordinator* coordinator) {
    coordinator->buttonGrid.decodeInterrupt([&](const ButtonState& bs) {
        if (bs[Pins::PlusOne].changed() && bs[Pins::PlusOne].pressed()) {
            state->score++;
            coordinator->display.print(state->score);
        }
        if (bs[Pins::PlusFive].changed() && bs[Pins::PlusFive].pressed()) {
            state->score += 5;
            coordinator->display.print(state->score);
        }
        if (bs[Pins::MinusOne].changed() && bs[Pins::MinusOne].pressed()) {
            state->score--;
            coordinator->display.print(state->score);
        }
        if (bs[Pins::Ok].changed() && bs[Pins::Ok].pressed()) {
            state->whosTurn = otherPeer(coordinator, state);
            coordinator->wifi.sendBroadcast(std::to_string(state->score).c_str());
            coordinator->display.print(state->score);
        }
        if (bs[Pins::Add].changed() && bs[Pins::Add].pressed()) {
            coordinator->wifi.shutdown();
            performOTAUpdate(&coordinator->display);
            coordinator->wifi.start();
        }
    });
}

void updatePeerList(GameState* state, Coordinator* coordinator) {
    state->peers = coordinator->wifi.getPeers();
}

void onNewPeer(GameState* state, const Event& e, Coordinator* coordinator) {
    updatePeerList(state, coordinator);
}

void onMessageReceived(GameState* state, const Event& e, Coordinator* coordinator) {
    state->score = std::stoi(e.messageReceived.wifiMessage);
    state->whosTurn = coordinator->wifi.getMyPeerId();
    Serial.printf(
        "[Received from=%i] [%s]\n", e.messageReceived.peerId, e.messageReceived.wifiMessage);
}

void onLostPeer(GameState* state, const Event&, Coordinator* coordinator) {
    updatePeerList(state, coordinator);
}

volatile uint32_t events;


void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch(e.type) {
        case EventType::ButtonPressed:
                onButtonPress(this, e.press, coordinator); break;
        case EventType::WifiConnected:
                break;
        case EventType::NewPeer:
            onNewPeer(this, e, coordinator); break;
        case EventType::LostPeer:
            onLostPeer(this, e, coordinator); break;
        case EventType::MessageReceived:
            onMessageReceived(this, e, coordinator); break;
        case EventType::StateUpdate:
            break;
    }

    events++;

    if (coordinator->state.peers.size() < 2 && (events % 80 > 60)) {
        char buf[4] = "@=0";
        buf[2] = '0' + coordinator->state.peers.size();
        coordinator->display.print(buf);
        coordinator->rotaryEncoder.lightOff();
    } else if (coordinator->state.whosTurn == coordinator->wifi.getMyPeerId()) {
        coordinator->rotaryEncoder.lightOn();
        coordinator->display.print("GO!");
    } else {
        char buf[4] = "!=0";
        buf[2] = '0' + events%10;
        coordinator->display.print(score);
        coordinator->rotaryEncoder.lightOff();
        // coordinator->rotaryEncoder.lightOff();
        // coordinator->display.print(events%80);
    }
}

