#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Event.hpp>
#include <BoardRole.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <Arduino.h>

class GameState {
public:
  GameState();

  void handleEvent(const Event& e, class Coordinator* coordinator);
  void restore();
  bool persist() const;
  uint32_t nextOperationId();
  void heartbeat(class Coordinator* coordinator);
  void refreshDisplays(class Coordinator* coordinator) const;

  int myScore;
  BoardRole whosTurn;
  std::array<int32_t, 4> scores;
  uint8_t connectedMask;
  uint8_t rosterMask;
  bool gameStarted;
  uint32_t gameId;
  uint32_t term;
  uint32_t version;
  uint32_t leaderId;
  bool leaderless;
  std::array<uint32_t, 4> lastOperation;
  uint32_t localOperation;
  uint32_t lastReplicationMs;
  uint32_t lastRejoinDisplayMs;
  uint32_t pendingOperation;
  int pendingScore;
  bool pendingPass;
  String pendingMessage;
  uint32_t pendingSentMs;
  uint32_t lastActivitySentMs;
  std::atomic<uint32_t> rotaryPressStartedMs;
};

#endif // GAME_STATE_H
