#include <Coordinator.hpp>
#include <GameState.hpp>
#include <Messages.hpp>
#include <utils.hpp>

GameState::GameState()
    : myScore{0},
      whosTurn{BoardRole::Unknown},
      scores{{BoardRole::Player_Red, 0},
             {BoardRole::Player_Blue, 0},
             {BoardRole::Player_Green, 0},
             {BoardRole::Player_White, 0}},
      whosConnected{},
      gameStarted{false} {}

BoardRole getNextTurn(BoardRole currentRole, GameState* state) {
    // Turn order: RED -> BLUE -> GREEN -> WHITE -> RED (loops)
    // But skip players that aren't connected
    BoardRole nextRole;
    switch (currentRole) {
        case BoardRole::Player_Red:
            nextRole = BoardRole::Player_Blue;
            break;
        case BoardRole::Player_Blue:
            nextRole = BoardRole::Player_Green;
            break;
        case BoardRole::Player_Green:
            nextRole = BoardRole::Player_White;
            break;
        case BoardRole::Player_White:
            nextRole = BoardRole::Player_Red;
            break;
        default:
            nextRole = BoardRole::Player_Red;  // Default to RED
            break;
    }

    // Return the next connected player
    if (std::find(state->whosConnected.begin(), state->whosConnected.end(), nextRole) ==
        state->whosConnected.end()) {
        return getNextTurn(nextRole, state);
    }

    // Fallback: return the next role
    return nextRole;
}

void broadcastNextTurnMessage(GameState* state, Coordinator* coordinator) {
    // Only leader should call this
    if (coordinator->myRole() != BoardRole::Leader)
        return;

    Serial.printf("DEBUG: Current turn before change: %d (%s)\n",
                  (int)state->whosTurn,
                  getRoleConfig(getNodeIdForRole(state->whosTurn)).name.c_str());

    BoardRole nextRole = getNextTurn(state->whosTurn, state);
    uint32_t nextNodeId = getNodeIdForRole(nextRole);
    BoardRoleConfig nextRoleConfig = getRoleConfig(nextNodeId);

    Serial.printf("DEBUG: Next turn calculated: %d (%s), NodeID: %u\n",
                  (int)nextRole,
                  nextRoleConfig.name.c_str(),
                  nextNodeId);

    if (nextNodeId != 0) {
        state->whosTurn = nextRole;  // Update current turn
        String nextPlayerName = getRoleConfig(nextNodeId).name.c_str();

        TurnMessage turnMsg(nextNodeId, nextPlayerName);
        String jsonStr = turnMsg.toJson();

        coordinator->wifi.sendTo(nextNodeId, jsonStr);

        Serial.printf(
            "Broadcasting next turn: %s (NodeID: %u)\n", nextPlayerName.c_str(), nextNodeId);

        // TODO: Update brightness for leader displays
        // updateDisplayBrightness();
    } else {
        Serial.printf("Warning: Next player role %d not found in connected nodes\n", (int)nextRole);
    }
}


void onButtonPress(GameState* state, const ButtonPressEvent& e, Coordinator* coordinator) {
    Serial.printf("DEBUG: Button pressed event started\n");

    uint8_t intPin = coordinator->buttonGrid.buttonGpio.getLastInterruptPin();
    uint8_t intVal = coordinator->buttonGrid.buttonGpio.getCapturedInterrupt();
    coordinator->buttonGrid.buttonGpio.clearInterrupts();

    if (e.buttonName == ButtonName::RotaryEncoder) {
        if (coordinator->rotaryEncoder.pressed()) {
            Serial.printf("DEBUG: Rotary encoder pressed\n");
            // OK button: Submit current score AND pass turn
            uint32_t myNodeId = coordinator->wifi.getMyPeerId();
            PlayerMessage msg(state->myScore, true, myNodeId);  // Pass actual nodeId
            String jsonStr = msg.toJson();
            uint32_t leaderNodeId = getNodeIdForRole(BoardRole::Leader);
            coordinator->wifi.sendTo(leaderNodeId, jsonStr);
            Serial.printf("Sent score update with turn pass: %s\n", jsonStr.c_str());

            // Reset current score to 0 after submission
            state->myScore = 0;
            coordinator->rotaryEncoder.reset();

            coordinator->display1.print("----");
            return;
        } else {
            // Reading the delta clears the interrupt.
            int32_t rotaryDelta = coordinator->rotaryEncoder.delta();
            Serial.printf("DEBUG: Rotary encoder delta: %d\n", rotaryDelta);

            if (rotaryDelta == 0) {
                return;
            }

            state->myScore += rotaryDelta;

            coordinator->display1.print(
                strFormat("%d", state->myScore));  // Will correctly show negative numbers
            Serial.printf("Display updated to %d for %s (reason: building score)\n",
                          state->myScore,
                          coordinator->myRoleConfig()->name.c_str());
            return;
        }
    }
 
    Serial.printf("DEBUG: Button pressed: intPin=%d, intVal=%d\n", intPin, intVal);

    if (intVal == ButtonGrid::intValReleased || intVal == ButtonGrid::intValReleased2) {
        // Handle leader board buttons (only 4 buttons: pins 0-3)
        if (coordinator->myRole() == BoardRole::Leader) {
            if (intPin == 0 && !state->gameStarted) {  // Button 0 starts the game
                state->gameStarted = true;
                broadcastNextTurnMessage(state, coordinator);

                std::map<BoardRole, HT16Display*> roleToDisplayMap = {
                    {BoardRole::Player_Red, &coordinator->display1},
                    {BoardRole::Player_Blue, &coordinator->display2},
                    {BoardRole::Player_Green, &coordinator->display3},
                    {BoardRole::Player_White, &coordinator->display4}};

                for (const auto& role : state->whosConnected) {
                    BoardRoleConfig thisRoleConfig = getRoleConfig(getNodeIdForRole(role));
                    roleToDisplayMap[role]->print(0);
                }

                return;
            }
            // TODO: Implement: Reset
        }
        // Handle player board buttons
        else {
            if (intPin == ButtonGrid::add) {
                // ADD button: Submit current score without passing turn (can be done anytime)
                if (state->myScore != 0) {
                    uint32_t myNodeId = coordinator->wifi.getMyPeerId();
                    PlayerMessage msg(state->myScore, false, myNodeId);  // Pass actual nodeId
                    String jsonStr = msg.toJson();
                    uint32_t leaderNodeId = getNodeIdForRole(BoardRole::Leader);
                    coordinator->wifi.sendTo(leaderNodeId, jsonStr);
                    Serial.printf("Sent score update (no turn pass): %s\n", jsonStr.c_str());

                    // Reset current score to 0 after submission
                    state->myScore = 0;
                    coordinator->rotaryEncoder.reset();

                    // TODO: Keep BEEF displayed if it's still my turn
                    // if (state->gameStarted && getNodeIdForRole(currentTurn) == myNodeId) {
                    //     display->print("BEEF");
                    //     Serial.printf("Keeping BEEF display - still my turn after ADD\n");
                    // }
                } else {
                    Serial.printf("No score to add\n");
                }
            } else if (intPin == ButtonGrid::negone) {
                state->myScore--;
            } else if (intPin == ButtonGrid::plusone) {
                state->myScore++;
            } else if (intPin == ButtonGrid::plusfive) {
                state->myScore += 5;
            } else if (intPin == ButtonGrid::okPin) {
                // OK button: Submit current score AND pass turn
                uint32_t myNodeId = coordinator->wifi.getMyPeerId();
                PlayerMessage msg(state->myScore, true, myNodeId);  // Pass actual nodeId
                String jsonStr = msg.toJson();
                uint32_t leaderNodeId = getNodeIdForRole(BoardRole::Leader);
                coordinator->wifi.sendTo(leaderNodeId, jsonStr);
                Serial.printf("Sent score update with turn pass: %s\n", jsonStr.c_str());

                // Reset current score to 0 after submission
                state->myScore = 0;
                coordinator->rotaryEncoder.reset();
            }
            // Only update display if we're building a score or have reset to 0
            // Don't overwrite BEEF or other turn indicators
            if (intPin == ButtonGrid::negone || intPin == ButtonGrid::plusone ||
                intPin == ButtonGrid::plusfive ||
                (intPin == ButtonGrid::add && state->myScore == 0) ||
                (intPin == ButtonGrid::okPin && state->myScore == 0)) {

                // Display "----" when currentScore is 0, otherwise show the score (including
                // negative)
                if (state->myScore == 0) {
                    coordinator->display1.print("----");
                    Serial.printf("Display updated to ---- for %s (reason: score=0)\n",
                                  coordinator->myRoleConfig()->name.c_str());
                } else {
                    coordinator->display1.print(
                        strFormat("%d", state->myScore));  // Will correctly show negative numbers
                    Serial.printf("Display updated to %d for %s (reason: building score)\n",
                                  state->myScore,
                                  coordinator->myRoleConfig()->name.c_str());
                }
            } else {
                Serial.printf("Display NOT updated for %s (preserving current display)\n",
                              coordinator->myRoleConfig()->name.c_str());
            }
        }
    }
}


void updateLeaderboardConnDisplay(GameState* state, Coordinator* coordinator) {
    // Update leaderboard display
    std::map<BoardRole, HT16Display*> roleToDisplayMap = {
        {BoardRole::Player_Red, &coordinator->display1},
        {BoardRole::Player_Blue, &coordinator->display2},
        {BoardRole::Player_Green, &coordinator->display3},
        {BoardRole::Player_White, &coordinator->display4}};

    for (const auto& role : state->whosConnected) {
        BoardRoleConfig thisRoleConfig = getRoleConfig(getNodeIdForRole(role));
        roleToDisplayMap[role]->print(thisRoleConfig.name.c_str());
    }
}

void onMessageReceived(GameState* state, const Event& e, Coordinator* coordinator) {
    uint32_t from = e.messageReceived.peerId;
    String message = e.messageReceived.wifiMessage;
    Serial.printf("[Received from=%u] [%s]\n", from, message.c_str());

    if (PlayerMessage::isPlayerMessage(message)) {
        PlayerMessage playerMsg = PlayerMessage::fromJson(message);
        BoardRoleConfig fromRoleConfig = getRoleConfig(playerMsg.fromNodeId);

        if (coordinator->myRole() != BoardRole::Leader) {
            Serial.printf("DEBUG: Ignoring player message from %s (not leader)\n",
                          fromRoleConfig.name.c_str());
            return;
        }

        // TODO: the from is wrong, type conversion error?
        // Validate that the message's fromNodeId matches the actual sender
        if (playerMsg.fromNodeId != from) {
            Serial.printf(
                "WARNING: Message fromNodeId (%u) doesn't match sender (%u). Using sender.\n",
                playerMsg.fromNodeId,
                from);
            // Continue processing with the correct 'from' value
        }

        if (playerMsg.fromNodeId == 0) {
            Serial.printf("WARNING: Message fromNodeId is 0. Ignoring.\n");
            return;
        }

        Serial.printf("Parsed PlayerMessage: Score=%d, TurnPassed=%s, FromNode=%u (%s)\n",
                      playerMsg.score,
                      playerMsg.turnPassed ? "YES" : "NO",
                      playerMsg.fromNodeId,
                      fromRoleConfig.name.c_str());

        // IGNORE score submissions before game starts
        if (!state->gameStarted) {
            Serial.printf("Game not started yet - ignoring score submission from %s\n",
                          fromRoleConfig.name.c_str());

            if (coordinator->myRole() == BoardRole::Leader) {
                if (std::find(state->whosConnected.begin(),
                              state->whosConnected.end(),
                              fromRoleConfig.role) == state->whosConnected.end()) {
                    Serial.printf("DEBUG: Adding %s to connected players\n",
                                  fromRoleConfig.name.c_str());
                    state->whosConnected.push_back(fromRoleConfig.role);
                    updateLeaderboardConnDisplay(state, coordinator);
                }
            }

            return;  // Exit early, don't process the score
        }

        if (coordinator->myRole() == BoardRole::Leader) {
            // Update leaderboard - use 'from' (the actual sender) not playerMsg.fromNodeId
            state->scores[fromRoleConfig.role] += playerMsg.score;

            Serial.printf("Updated leaderboard for %s: Total=%d, Offset=%d\n",
                          fromRoleConfig.name.c_str(),
                          state->scores[fromRoleConfig.role],
                          playerMsg.score);

            // Display on leader board (only if leader)
            std::map<BoardRole, HT16Display*> roleToDisplayMap = {
                {BoardRole::Player_Red, &coordinator->display1},
                {BoardRole::Player_Blue, &coordinator->display2},
                {BoardRole::Player_Green, &coordinator->display3},
                {BoardRole::Player_White, &coordinator->display4}};

            roleToDisplayMap[fromRoleConfig.role]->print(
                strFormat("%d", state->scores[fromRoleConfig.role]));

            // Turn management: if player passed turn, broadcast next player
            if (playerMsg.turnPassed && state->gameStarted) {
                Serial.printf("Player %s passed their turn!\n", fromRoleConfig.name.c_str());

                // Only process turn passing if it's actually their turn
                uint32_t currentTurnNodeId = getNodeIdForRole(state->whosTurn);
                Serial.printf("DEBUG: Current turn role: %d, NodeID: %u\n",
                              (int)state->whosTurn,
                              currentTurnNodeId);
                Serial.printf("DEBUG: Message sender NodeID: %u\n", from);

                if (from == currentTurnNodeId) {
                    Serial.printf("Turn pass confirmed for current player\n");
                    broadcastNextTurnMessage(state, coordinator);
                } else {
                    Serial.printf(
                        "Turn pass ignored - not current player's turn (current: %u, sender: %u)\n",
                        currentTurnNodeId,
                        from);
                }
            } else if (!playerMsg.turnPassed && state->gameStarted) {
                // Player added score but didn't pass turn - remind all players of current turn
                Serial.printf("Player %s added score but didn't pass turn - refreshing displays\n",
                              fromRoleConfig.name.c_str());

                // Send current turn reminder to all players
                // TODO: Do we actually need this?
                uint32_t currentTurnNodeId = getNodeIdForRole(state->whosTurn);
                String currentTurnName = getRoleConfig(currentTurnNodeId).name.c_str();
                TurnMessage turnMsg(currentTurnNodeId, currentTurnName);
                String jsonStr = turnMsg.toJson();
                coordinator->wifi.sendBroadcast(jsonStr);
                Serial.printf("Sent turn reminder: %s\n", jsonStr.c_str());
            }
        }
    } else if (TurnMessage::isTurnMessage(message)) {
        TurnMessage turnMsg = TurnMessage::fromJson(message);

        Serial.printf("Parsed TurnMessage: NextPlayer=%s (NodeID: %u)\n",
                      turnMsg.nextPlayerName.c_str(),
                      turnMsg.nextPlayerNodeId);
        Serial.printf("My NodeID: %u, My Role: %s\n",
                      coordinator->wifi.getMyPeerId(),
                      coordinator->myRoleConfig()->name.c_str());

        // Validate nodeId is not 0
        if (turnMsg.nextPlayerNodeId == 0) {
            Serial.printf("ERROR: Turn message has invalid nodeId 0!\n");
            return;
        }

        // Check if it's my turn
        if (turnMsg.nextPlayerNodeId == coordinator->wifi.getMyPeerId()) {
            Serial.println("IT'S MY TURN! Displaying BEEF");
            coordinator->display1.print("BEEF");
            Serial.printf("Set display to BEEF for %s\n",
                          coordinator->myRoleConfig()->name.c_str());
        } else {
            Serial.printf("It's %s's turn (not mine), keeping current display\n",
                          turnMsg.nextPlayerName.c_str());
            // When it's not my turn and game has started, show "----"
            if (state->gameStarted && coordinator->myRole() != BoardRole::Leader) {
                coordinator->display1.print("----");
                Serial.printf("Set display to ---- (not my turn)\n");
            }
        }

        // Update brightness for all displays
        // TODO: update brightness based on turn
        // updateDisplayBrightness();
    } else {
        Serial.println("Received unknown message format");
    }
}

void onNewPeer(GameState* state, const Event& e, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }

    uint32_t newPeerId = e.newPeer.peerId;
    Serial.printf("New peer connected: %u\n", newPeerId);

    if (state->gameStarted) {
        Serial.printf("Game already started, ignoring new peer\n");
        return;
    }

    BoardRoleConfig config = getRoleConfig(newPeerId);
    if (config.role != BoardRole::Leader) {
        state->whosConnected.push_back(config.role);
        updateLeaderboardConnDisplay(state, coordinator);
    }
}

void onLostPeer(GameState* state, const Event& e, Coordinator* coordinator) {
    // TODO: Implement later
}

void GameState::handleEvent(const Event& e, Coordinator* coordinator) {
    switch (e.type) {
        case EventType::ButtonPressed:
            onButtonPress(this, e.press, coordinator);
            break;
        case EventType::WifiConnected:
            break;
        case EventType::NewPeer:
            onNewPeer(this, e, coordinator);
            break;
        case EventType::LostPeer:
            onLostPeer(this, e, coordinator);
            break;
        case EventType::MessageReceived:
            onMessageReceived(this, e, coordinator);
            break;
        case EventType::StateUpdate:
            break;
    }
}
