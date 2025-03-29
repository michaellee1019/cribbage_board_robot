#include <Coordinator.hpp>
#include <GameState.hpp>
#include <HWCDC.h>

GameState::GameState()
    : score(0), turn(TurnState::OpponentTurn)
{}

void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch(e.type) {
        case EventType::ButtonPressed: {
            Serial.println("ButtonPressed");
            uint8_t intPin = coordinator->buttonGrid.buttonGpio.getLastInterruptPin();   // Which pin caused it?
            uint8_t intVal = coordinator->buttonGrid.buttonGpio.getCapturedInterrupt();  // What was the level?
            // if (intPin != MCP23XXX_INT_ERR) {
            //     coordinator->display.print(strFormat("%d %2x", intPin, intVal));
            // }
            coordinator->buttonGrid.buttonGpio.clearInterrupts();
            Serial.printf("Button %i Changed %i\n", intPin, intVal);

            if(turn == TurnState::MyTurn) {
                score++;
                //                display.update(score);
            }
        } break;
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