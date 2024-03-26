#include "ArduinoSTL.h"
#include <Message.hpp>

// PlayerBoard response updates in response to the request.
void StateRefreshResponse::update(const StateRefreshRequest& lastReceived) {
    //    const bool advancedTurn = lastReceived.turnNumber() > this->turnNumber;
    if (this->commit) {
        this->commit = false;
        this->delta.reset();
    }
    if (this->passTurn) {
        this->passTurn = false;
        this->delta.reset();
    }

    this->turnNumber = lastReceived.turnNumber();
}
bool StateRefreshResponse::isPlayerAndPassedTurn(PlayerNumberT player) const {
    return this->fromPlayer == player && this->passedTurn();
}

ScoreT StateRefreshResponse::myScoreDelta() const {
    return this->delta.scoreDelta;
}
bool StateRefreshResponse::hasScoreDelta() const {
    return this->delta.scoreDelta != 0;
}
void StateRefreshResponse::addScore(const ScoreT n)  {
    this->delta.scoreDelta += n;
}
void StateRefreshResponse::setCommit(bool commitVal)  {
    this->commit = commitVal;
}
void StateRefreshResponse::setPassTurn(bool pass) {
    this->passTurn = pass;
}

void StateRefreshRequest::update(StateRefreshResponse* responses, PlayerNumberT nResponses) {
    bool advanceTurn = false;
    for (PlayerNumberT i=0; i < nResponses; ++i) {
        const StateRefreshResponse& response = responses[i];
        if (response.committed() || response.passedTurn()) {
            this->state.scores[i] += response.myScoreDelta();
        }
        if (response.isPlayerAndPassedTurn(this->state.whosTurn)) {
            advanceTurn = true;
        }
    }
    if (advanceTurn) {
        this->state.turnNumber++;
        this->state.whosTurn = (this->state.whosTurn + 1) % MAX_PLAYERS;
    }
    Serial.print("Turn number ");
    Serial.print(this->state.turnNumber);
    Serial.print(", whos turn=");
    Serial.print(this->state.whosTurn);
    Serial.println();
}
