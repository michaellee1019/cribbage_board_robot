#include <Message.hpp>

void StateRefreshResponse::update(const StateRefreshRequest& lastReceived) {
    if (this->commit && lastReceived.turnNumber() > this->turnNumber) {
        this->passTurn = false;
        this->commit = false;
        this->turnNumber = lastReceived.turnNumber();
        this->delta.reset();
    }
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

void StateRefreshRequest::update(StateRefreshResponse* responses, PlayerNumberT nResponses) {
    bool advanceTurn = false;
    for (PlayerNumberT i=0; i < nResponses; ++i) {
        const StateRefreshResponse& response = responses[i];
        if (response.isPlayerAndPassedTurn(i)) {
            advanceTurn = true;
        }
    }
    if (advanceTurn) {
        this->state.turnNumber++;
        this->state.whosTurn = (this->state.whosTurn + 1) % MAX_PLAYERS;
    }
}
