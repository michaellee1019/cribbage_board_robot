#include <Message.hpp>

// PlayerBoard response updates in response to the request.
void StateRefreshResponse::update(const StateRefreshRequest& lastReceived) {
    //    const bool advancedTurn = lastReceived.turnNumber() > this->turnNumber;
    if (this->commit) {
        this->commit = false;
        this->resetScoreDelta();
    }
    if (this->passTurn) {
        this->passTurn = false;
        this->resetScoreDelta();
    }

    //    this->turnNumber = lastReceived.turnNumber();
}
bool StateRefreshResponse::isPlayerAndPassedTurn(PlayerNumberT player) const {
    return this->fromPlayer == player && this->passedTurn();
}

ScoreT StateRefreshResponse::myScoreDelta() const {
    return this->scoreDelta;
}
bool StateRefreshResponse::hasScoreDelta() const {
    return this->scoreDelta != 0;
}
void StateRefreshResponse::addScore(const ScoreT n) {
    this->scoreDelta += n;
}
void StateRefreshResponse::setCommit(bool commitVal) {
    this->commit = commitVal;
}
void StateRefreshResponse::setPassTurn(bool pass) {
    this->passTurn = pass;
}

void StateRefreshRequest::update(StateRefreshResponse const* responses,
                                 const PlayerNumberT nResponses,
                                 const PlayerNumberT maxActivePlayerIndex) {
    bool advanceTurn = false;
    for (PlayerNumberT i = 0; i < nResponses; ++i) {
        const StateRefreshResponse& response = responses[i];
        if (response.committed() || response.passedTurn()) {
            this->scores[i] += response.myScoreDelta();
        }
        if (response.isPlayerAndPassedTurn(this->whosTurnV)) {
            advanceTurn = true;
        }
    }
    if (advanceTurn) {
        this->turnNumber++;
        this->whosTurnV = (this->whosTurnV + 1) % (maxActivePlayerIndex + 1);
    }
    //    Serial.print("Turn number ");
    //    Serial.print(this->state.turnNumber);
    //    Serial.print(", whos turn=");
    //    Serial.print(this->state.whosTurn);
    //    Serial.print(", maxActivePlayerIndex=");
    //    Serial.print(maxActivePlayerIndex);
    //    Serial.println();
}
