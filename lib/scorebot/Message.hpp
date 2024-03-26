#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Types.hpp>

#include <Arduino.h>


struct GameState {
    TurnNumberT turnNumber{-1};
    PlayerNumberT whosTurn{-1};
    ScoreT scores[MAX_PLAYERS]{};
};

struct StateDelta {
    ScoreT scoreDelta{0};

    void reset() {
        scoreDelta = 0;
    }
};

class StateRefreshRequest {
    GameState state{};

public:

//    [[nodiscard]]
//    auto turnNumber() const {
//        return state.turnNumber;
//    }

    [[nodiscard]]
    bool myTurn() const {
        return this->state.whosTurn == BOARD_ID;
    }

    [[nodiscard]]
    PlayerNumberT whosTurn() const {
        return this->state.whosTurn;
    }

    [[nodiscard]]
    auto getPlayerScore(PlayerNumberT player) const {
        return state.scores[player];
    }

    void update(class StateRefreshResponse* responses, PlayerNumberT nResponses);
};

class StateRefreshResponse {
    PlayerNumberT fromPlayer{BOARD_ID};
    bool passTurn{false};
    bool commit{false};
    StateDelta delta{};

public:

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
static_assert(sizeof(StateRefreshRequest) <= 32,
              "StateRefreshRequest max struct size");


#endif  // MESSAGE_HPP
