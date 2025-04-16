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

void onButtonPress(GameState* state, const ButtonPressEvent& e, Coordinator* coordinator) {
    state->score++;

    uint8_t whichPin  = coordinator->buttonGrid.buttonGpio.getLastInterruptPin();
    // int32_t rotaryPosition = coordinator->rotaryEncoder.position();
    // uint8_t pinValues = coordinator->buttonGrid.buttonGpio.getCapturedInterrupt();
    if (whichPin == ButtonGrid::okPin) {
        state->whosTurn = otherPeer(coordinator, state);
        coordinator->wifi.sendBroadcast(std::to_string(state->score).c_str());
    }
    if (whichPin == ButtonGrid::add) {
        coordinator->wifi.shutdown();
        performOTAUpdate();
        coordinator->wifi.start();
    }
    coordinator->buttonGrid.buttonGpio.clearInterrupts();
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
        char buf[4] = "#=0";
        buf[2] = '0' + events%10;
        coordinator->display.print(buf);
        coordinator->rotaryEncoder.lightOff();
        // coordinator->rotaryEncoder.lightOff();
        // coordinator->display.print(events%80);
    }
}

