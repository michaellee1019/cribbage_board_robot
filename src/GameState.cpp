#include <Coordinator.hpp>
#include <GameState.hpp>

// #include <map>
// std::map<int, String> playerNumberMap = {
//     {1, "RED"},
//     {2, "BLUE"},
//     {3, "GREN"},
//     {4, "WHIT"},
// };

// void loop(GameState* const state) {
//     if (!state->interrupted) {
//         return;
//     }
//     auto pressed = !ss.digitalRead(SS_SWITCH);
//     auto val = ss.getEncoderPosition();
//
//     // initialize player selection
//     if (!state->isLeaderboard && state->playerNumber == 0) {
//         if (val > -1) {
//             display->print(playerNumberMap[val]);
//         }
//
//         if (pressed) {
//             state->playerNumber = val;
//             Serial.printf("Player set to: %s\n", playerNumberMap[state->playerNumber].c_str());
//         }
//     }
//     state->interrupted = false;
// }
GameState::GameState()
    : score{},
      whosTurn{0}
{}

uint8_t otherPeer(Coordinator* coordinator, GameState* state) {
    for (const auto& peer : state->peers) {
        if (peer != coordinator->wifi.getMyPeerId()) {
            return peer;
        }
    }
    return 0;
}

void onButtonPress(GameState* state, const ButtonPressEvent& e, Coordinator* coordinator) {
    // Serial.println("ButtonPressed");

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

    coordinator->buttonGrid.buttonGpio.clearInterrupts();
}

void onNewPeer(GameState* state, const Event& e, Coordinator* coordinator) {
    state->peers.insert(e.newPeer.peerId);
    // Serial.printf("%i has new peer %i\n", coordinator->wifi.getMyPeerId(), e.newPeer.peerId);
}

void onMessageReceived(GameState* state, const Event& e, Coordinator* coordinator) {
    state->score = std::stoi(e.messageReceived.wifiMessage);
    state->whosTurn = coordinator->wifi.getMyPeerId();
    Serial.printf("[Received from=%i] [%s]\n", e.messageReceived.peerId, e.messageReceived.wifiMessage);
}

void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch(e.type) {
        case EventType::ButtonPressed: onButtonPress(this, e.press, coordinator); break;
        case EventType::WifiConnected: break;
        case EventType::NewPeer: onNewPeer(this, e, coordinator); break;
        case EventType::MessageReceived: onMessageReceived(this, e, coordinator); break;
        case EventType::StateUpdate: break;
    }

    Serial.printf("GameState::handleEvent whosTurn=%i myPeer=%i\n", coordinator->state.whosTurn, coordinator->wifi.getMyPeerId());
    if (coordinator->state.whosTurn == coordinator->wifi.getMyPeerId()) {
        coordinator->rotaryEncoder.lightOn();
        coordinator->display.print("GO!");
    } else {
        coordinator->rotaryEncoder.lightOff();
        coordinator->display.clear();
    }
}

