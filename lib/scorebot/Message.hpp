#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Types.hpp>

#include <Arduino.h>


struct GameState {
    bool populated{false};
    TurnNumberT turnNumber{-1};
    PlayerNumberT whosTurn{-1};
    ScoreT scores[MAX_PLAYERS]{};
};

struct StateDelta {
    bool populated{false};
    PlayerNumberT fromPlayer{-1};
    ScoreT scoreDelta{0};
};

class StateRefreshRequest {
    bool populated;
    TimestampT requestedAtTime;
    const TimestampT startupGeneration;
    GameState state{};

public:

    explicit StateRefreshRequest()
        : StateRefreshRequest(0, false)
    {}


    explicit StateRefreshRequest(TimestampT startupGeneration, bool populated=false)
        : populated{populated},
          requestedAtTime{0},
          startupGeneration{startupGeneration},
          state{}
    {}

    [[nodiscard]]
    bool myTurn() const {
        return populated && this->state.whosTurn == BOARD_ID;
    }

    [[nodiscard]]
    PlayerNumberT whosTurn() const {
        return this->state.whosTurn;
    }

    [[nodiscard]]
    auto getPlayerScore(PlayerNumberT player) const {
        return state.scores[player];
    }

    void update(class StateRefreshResponse* responses, size_t nResponses);
};

class StateRefreshResponse {
    bool populated {false};
    PlayerNumberT fromPlayer{BOARD_ID};
    TimestampT respondedAtTime{0};
    bool passTurn{false};
    bool commit{false};
    StateDelta delta{};

public:

    [[nodiscard]]
    ScoreT myScoreDelta() const {
        return this->delta.scoreDelta;
    }

    void addScore(const ScoreT n) {
        this->delta.scoreDelta += n;
    }

    // PlayerBoard received this state update nextRequest.
    void update(const StateRefreshRequest& request);

    void setCommit(bool commitVal) {
        this->commit = commitVal;
    }
};

// I think this is a requirement of the NRF stack.
static_assert(sizeof(StateRefreshRequest) <= 32,
              "StateRefreshRequest max struct size");


#endif  // MESSAGE_HPP
