#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Types.hpp>

class StateRefreshRequest {
    TurnNumberT turnNumber;
    PlayerNumberT whosTurnV;
    ScoreT scores[MAX_PLAYERS];

public:
    //    [[nodiscard]]
    //    auto turnNumber() const {
    //        return state.turnNumber;
    //    }

    explicit StateRefreshRequest() : turnNumber{-1}, whosTurnV{-1}, scores{} {}

    [[nodiscard]]
    bool myTurn() const {
        return this->whosTurnV == BOARD_ID;
    }

    [[nodiscard]]
    PlayerNumberT whosTurn() const {
        return this->whosTurnV;
    }

    [[nodiscard]]
    auto getPlayerScore(const PlayerNumberT player) const {
        return this->scores[player];
    }

    void update(class StateRefreshResponse const* responses,
                PlayerNumberT nResponses,
                PlayerNumberT maxActivePlayerIndex);
};

class StateRefreshResponse {
    PlayerNumberT fromPlayer;
    bool passTurn;
    bool commit;
    ScoreT scoreDelta;

public:
    explicit StateRefreshResponse()
        : fromPlayer{BOARD_ID}, passTurn{false}, commit{false}, scoreDelta{0} {}

    void resetScoreDelta() {
        this->scoreDelta = 0;
    }

    [[nodiscard]]
    bool committed() const {
        return commit;
    }
    [[nodiscard]]
    bool passedTurn() const {
        return passTurn;
    }

    [[nodiscard]]
    ScoreT myScoreDelta() const;

    [[nodiscard]]
    bool hasScoreDelta() const;

    void addScore(ScoreT n);

    void setCommit(bool commitVal);

    [[nodiscard]]
    bool isPlayerAndPassedTurn(PlayerNumberT i) const;

    void update(const StateRefreshRequest& request);
    void setPassTurn(bool passTurn);
};

// I think this is a requirement of the NRF stack.
static_assert(sizeof(StateRefreshRequest) <= 32, "StateRefreshRequest max struct size");


#endif  // MESSAGE_HPP
