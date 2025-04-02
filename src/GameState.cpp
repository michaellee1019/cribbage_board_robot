#include <Coordinator.hpp>
#include <GameState.hpp>

// #include <map>
// std::map<int, String> playerNumberMap = {
//     {1, "RED"},
//     {2, "BLUE"},
//     {3, "GREN"},
//     {4, "WHIT"},
// };

GameState::GameState()
    : score{},
      whosTurn{0}
{}

uint32_t otherPeer(Coordinator* coordinator, GameState* state) {
    for (const auto& peer : state->peers) {
        if (peer != coordinator->wifi.getMyPeerId()) {
            return peer;
        }
    }
    return 0;
}

void onButtonPress(GameState* state, const ButtonPressEvent& e, Coordinator* coordinator) {
    int32_t pos = coordinator->rotaryEncoder.position();

    uint8_t intPin = coordinator->buttonGrid.buttonGpio.getLastInterruptPin();   // Which pin caused it?
    uint8_t intVal = coordinator->buttonGrid.buttonGpio.getCapturedInterrupt();  // What was the level?
    Serial.printf("intPin=%x, intVal=%x, pos=%i\n", intPin, intVal, pos);
    // if (intPin != MCP23XXX_INT_ERR) {
    //     coordinator->display.print(strFormat("%d %2x", intPin, intVal));
    // }
    // Serial.printf("Button %i Changed %i; pos %i\n", intPin, intVal, pos);
    state->score++;

    auto wasOkay = intPin == ButtonGrid::okPin;
    if (wasOkay) {
        state->whosTurn = otherPeer(coordinator, state);
        coordinator->wifi.sendBroadcast(std::to_string(state->score).c_str());
    }
    Serial.printf("WasOkay=%d, WhosTurn=%d, myPeerId=%d, otherPeer=%d\n",
        wasOkay, state->whosTurn, coordinator->wifi.getMyPeerId(),
        otherPeer(coordinator, state));

    coordinator->buttonGrid.buttonGpio.clearInterrupts();
}

void onNewPeer(GameState* state, const Event& e, Coordinator* coordinator) {
    state->peers = coordinator->wifi.getPeers();
    // Serial.printf("%i has new peer %i\n", coordinator->wifi.getMyPeerId(), e.newPeer.peerId);
}

void onMessageReceived(GameState* state, const Event& e, Coordinator* coordinator) {
    state->score = std::stoi(e.messageReceived.wifiMessage);
    state->whosTurn = coordinator->wifi.getMyPeerId();
    Serial.printf(
        "[Received from=%i] [%s]\n", e.messageReceived.peerId, e.messageReceived.wifiMessage);
}

void lostPeer(GameState* state, const Event&, Coordinator* coordinator) {
    state->peers = coordinator->wifi.getPeers();
}

void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch(e.type) {
        case EventType::ButtonPressed: onButtonPress(this, e.press, coordinator); break;
        case EventType::WifiConnected: break;
        case EventType::NewPeer: onNewPeer(this, e, coordinator); break;
        case EventType::LostPeer: lostPeer(this, e, coordinator); break;
        case EventType::MessageReceived: onMessageReceived(this, e, coordinator); break;
        case EventType::StateUpdate: break;
    }

    Serial.printf("GameState::handleEvent whosTurn=%i myPeer=%i, peersSize=%i\n",
        coordinator->state.whosTurn, coordinator->wifi.getMyPeerId(),
        coordinator->wifi.getPeers().size()
        );
    if (coordinator->state.peers.size() < 2) {
        coordinator->display.print(coordinator->state.peers.size());
        coordinator->rotaryEncoder.lightOff();
        return;
    }
    if (coordinator->state.whosTurn == coordinator->wifi.getMyPeerId()) {
        coordinator->rotaryEncoder.lightOn();
        coordinator->display.print("GO!");
    } else {
        coordinator->rotaryEncoder.lightOff();
        coordinator->display.clear();
    }
}

