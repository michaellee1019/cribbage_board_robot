#include <GameState.hpp>
#include <HWCDC.h>

GameState::GameState()
    : score(0), turn(TurnState::OpponentTurn)
{}

void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch(e.type) {
        case EventType::ButtonPressed:
            if(turn == TurnState::MyTurn) {
                score++;
//                display.update(score);
            }
        break;
        case EventType::WifiConnected:
            turn = TurnState::MyTurn;
//        display.showMessage(e.wifiMessage);
        break;
        case EventType::NewPeer:
            peers.insert(e.peerId);
            Serial.println("New peer");
            break;

    }
}