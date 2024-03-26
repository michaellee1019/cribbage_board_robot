#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Types.hpp>

struct GameState {
    TurnNumberT turnNumber;
    PlayerNumberT whosTurn;
    ScoreT scores[MAX_PLAYERS];

    explicit GameState()
        : turnNumber{-1},
          whosTurn{-1},
          scores{}
    {}
};

struct StateDelta {
    ScoreT scoreDelta;

    explicit StateDelta()
        : scoreDelta{0}
    {}

    void reset() {
        scoreDelta = 0;
    }
};

class StateRefreshRequest {
    GameState state;

public:

//    [[nodiscard]]
//    auto turnNumber() const {
//        return state.turnNumber;
//    }

    explicit StateRefreshRequest()
        : state{}
    {}

    [[nodiscard]]
    bool myTurn() const {
        return this->state.whosTurn == BOARD_ID;
    }

    [[nodiscard]]
    PlayerNumberT whosTurn() const {
        return this->state.whosTurn;
    }

    [[nodiscard]]
    auto getPlayerScore(const PlayerNumberT player) const {
        return state.scores[player];
    }

    void update(class StateRefreshResponse* responses, PlayerNumberT nResponses, PlayerNumberT maxActivePlayerIndex);
};

class StateRefreshResponse {
    PlayerNumberT fromPlayer;
    bool passTurn;
    bool commit;
    StateDelta delta;

public:

    explicit StateRefreshResponse()
        : fromPlayer{BOARD_ID},
          passTurn{false},
          commit{false},
          delta{}
    {}

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
