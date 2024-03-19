#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <Arduino.h>

using PlayerNumberT = int;
using ScoreT = int;
using TurnNumberT = int;
using TimestampT = decltype(millis());

template <typename T>
void print(const T& t) {
    Serial.print(t);
}

// TODO: Make this configurable.
static constexpr int N_DISPLAYS = 2;
static constexpr int N_PLAYERS = N_DISPLAYS;

struct GameState {
    bool populated{false};
    TurnNumberT turnNumber{-1};
    PlayerNumberT whosTurn{-1};
    ScoreT scores[N_PLAYERS]{};
};

struct StateDelta {
    bool populated{false};
    PlayerNumberT fromPlayer{-1};
    ScoreT scoreDelta{0};
};

struct StateRefreshRequest {
    bool populated{false};
    TimestampT startupGeneration{0};
    TimestampT requestedAtTime{0};
    GameState state{};

    [[nodiscard]]
    bool myTurn() const {
        return populated && this->state.whosTurn == BOARD_ID;
    }

    static StateRefreshRequest startup() {
        return {};
    }

    void update(struct StateRefreshResponse* responses);
};

struct StateRefreshResponse {
    bool populated {false};
    PlayerNumberT fromPlayer{BOARD_ID};
    TimestampT respondedAtTime{0};
    bool passTurn{false};
    bool commit{false};
    StateDelta delta{};

    static StateRefreshResponse startup() {
        return {};
    }

    void addScore(const ScoreT n) {
        this->delta.scoreDelta += n;
    }

    // PlayerBoard received this state update request.
    void update(const StateRefreshRequest& request);
};

// I think this is a requirement of the NRF stack.
static_assert(sizeof(StateRefreshRequest) <= 32,
              "StateRefreshRequest max struct size");


#endif  // MESSAGE_HPP
