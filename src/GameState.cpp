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

// https://stackoverflow.com/questions/111928/is-there-a-printf-converter-to-print-in-binary-format
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')


// TODO: rename to onButtonChange
void onButtonPress(GameState* state, const ButtonPressEvent& e, Coordinator* coordinator) {

    // Cool story, bro. Tell it again.
    //
    // const auto interruptState = coordinator->buttonGrid.resetInterrupts();
    // if (interruptState.ok() && interruptState.isPressed()) {
    // }
    // switch (interruptState) {
    //     case ButtonGrid::Ok_Press:
    //     case ButtonGrid::Ok_Release:
    // }

    uint8_t whichPin  = coordinator->buttonGrid.buttonGpio.getLastInterruptPin();
    // int32_t rotaryPosition = coordinator->rotaryEncoder.position();
    uint8_t pinValues = coordinator->buttonGrid.buttonGpio.getCapturedInterrupt();
    Serial.printf("pinValues = " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(pinValues));
    // Serial.printf("&= " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(pinValues & (1 << ButtonGrid::okPin)));
    auto okOnRelease = pinValues & (1 << ButtonGrid::okPin);
    auto plusOneRelease = pinValues & (1 << ButtonGrid::plusone);
    auto plusFiveRelease = pinValues & (1 << ButtonGrid::plusfive);
    auto negOneRelease = pinValues & (1 << ButtonGrid::negone);

    Serial.printf("whichPin = %d\n", whichPin);
    Serial.printf("plusOneRelease = %d\n", plusOneRelease);
        if (whichPin == ButtonGrid::plusone && !plusOneRelease) {
            state->score++;
            coordinator->display.print(state->score);
        }
        if (whichPin == ButtonGrid::plusfive && !plusFiveRelease) {
            state->score=state->score+5;
            coordinator->display.print(state->score);
        }
        if (whichPin == ButtonGrid::negone && !negOneRelease) {
            state->score--;
            coordinator->display.print(state->score);
        }

        // if (whichPin == ButtonGrid::okPin && okOnRelease) {
        //     Serial.println("okay button onRelease");
        // }
        if (whichPin == ButtonGrid::okPin && !okOnRelease) {
            //Serial.println("okay button onPress");
            state->whosTurn = otherPeer(coordinator, state);
            coordinator->wifi.sendBroadcast(std::to_string(state->score).c_str());
            coordinator->display.print(state->score);
        }

    if (whichPin == ButtonGrid::add) {
        coordinator->wifi.shutdown();
        performOTAUpdate(&coordinator->display);
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
        char buf[4] = "!=0";
        buf[2] = '0' + events%10;
        coordinator->display.print(score);
        coordinator->rotaryEncoder.lightOff();
        // coordinator->rotaryEncoder.lightOff();
        // coordinator->display.print(events%80);
    }
}

