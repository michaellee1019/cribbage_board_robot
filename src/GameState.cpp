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

void onButtonPress(GameState* state, const Event& e, Coordinator* coordinator) {
    Serial.println("ButtonPressed");

    auto pos = coordinator->rotaryEncoder.ss.getEncoderPosition();

    uint8_t intPin = coordinator->buttonGrid.buttonGpio.getLastInterruptPin();   // Which pin caused it?
    uint8_t intVal = coordinator->buttonGrid.buttonGpio.getCapturedInterrupt();  // What was the level?
    // if (intPin != MCP23XXX_INT_ERR) {
    //     coordinator->display.print(strFormat("%d %2x", intPin, intVal));
    // }
    coordinator->buttonGrid.buttonGpio.clearInterrupts();
    Serial.printf("Button %i Changed %i; pos %i\n", intPin, intVal, pos);
    state->score++;
    coordinator->wifi.sendBroadcast(std::to_string(state->score).c_str());
}

void onNewPeer(GameState* state, const Event& e, Coordinator* coordinator) {
    state->peers.insert(e.newPeer.peerId);
    Serial.printf("New peer %i\n", e.newPeer.peerId);
}

void onMessageReceived(GameState* state, const Event& e, Coordinator* coordinator) {
    auto score = std::stoi(e.messageReceived.wifiMessage);
    state->score = std::max(state->score, score);
    Serial.printf("[Received from=%i] [%s]\n", e.messageReceived.peerId, e.messageReceived.wifiMessage);
}

void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch(e.type) {
        case EventType::ButtonPressed: onButtonPress(this, e, coordinator); break;
        case EventType::WifiConnected: break;
        case EventType::NewPeer: onNewPeer(this, e, coordinator); break;
        case EventType::MessageReceived: onMessageReceived(this, e, coordinator); break;
    }

    coordinator->display.print(score);
}

