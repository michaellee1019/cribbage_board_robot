#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Event.hpp>
#include <BoardRole.hpp>
#include <ButtonGestureRules.hpp>
#include <GameRules.hpp>
#include <LeaderboardBroadcast.hpp>
#include <MaintenanceRules.hpp>
#include <PlayerUiRules.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <Arduino.h>

class GameState {
public:
  GameState();

  void handleEvent(const Event& e, class Coordinator* coordinator);
  void restore(uint32_t leader, uint32_t session);
  void selectSession(uint32_t leader, uint32_t session);
  void resumeAuthority(uint32_t identity);
  bool persist() const;
  static bool persistEncoded(const char* encoded);
  uint32_t nextOperationId();
  void heartbeat(class Coordinator* coordinator);
  void refreshDisplays(class Coordinator* coordinator);
  void refreshLeaderboardTurnFrame(class Coordinator* coordinator);
  scorebot::PlayerDisplayMode playerDisplayMode(BoardRole role) const;
  void syncLeaderboardSelection(class Coordinator* coordinator);
  bool hasPendingLocalScore() const;
  bool buttonGestureInProgress() const;
  void suppressHeldButtons(uint16_t pressed);
  scorebot::LeaderboardBroadcast leaderboardBroadcast() const;

  int myScore;
  BoardRole whosTurn;
  uint32_t turnStatusStartedMs;
  uint32_t lastTurnStatusFrame;
  std::array<int32_t, 4> scores;
  uint8_t connectedMask;
  uint8_t localControlMask;
  uint8_t lobbyEnabledMask;
  uint8_t lobbyExplicitMask;
  uint8_t sleepingMask;
  uint8_t rosterMask;
  bool gameStarted;
  uint32_t gameId;
  uint32_t sessionId{0};
  std::array<uint32_t, 4> controllerNodes{};
  std::array<uint32_t, 4> controllerOperations{};
  BoardRole pendingControllerRole{BoardRole::Unknown};
  uint32_t term;
  uint32_t version;
  uint32_t leaderId;
  bool leaderless;
  std::array<uint32_t, 4> lastOperation;
  uint8_t turnOrder{scorebot::kDefaultTurnOrder};
  scorebot::TurnCompletions completedTurns{};
  bool turnHistoryKnown{false};
  uint32_t localOperation;
  uint32_t lastReplicationMs;
  uint32_t lastLobbyDisplayMs;
  uint32_t pendingOperation;
  int pendingScore;
  bool pendingPass;
  String pendingMessage;
  uint32_t pendingSentMs;
  scorebot::TurnCompletions draftTurnBaseline{};
  std::array<int32_t, 4> leaderboardDeltas;
  BoardRole selectedLeaderboardRole;
  bool editingTurnOrder{false};
  uint8_t lobbyTurnOrder{scorebot::kDefaultTurnOrder};
  bool lobbyModeDirty;
  uint32_t lobbyModeChangedMs;
  uint16_t heldButtonMask;
  uint16_t suppressedButtonMask;
  bool invalidButtonGesture{false};
  scorebot::ButtonReleaseDebounce buttonReleaseDebounce;
  struct StagedButtonScore {
    uint32_t game;
    uint32_t session;
    BoardRole target;
    uint8_t pin;
    bool pairing;
    bool canScore;
  };
  scorebot::StagedScoreButtons<StagedButtonScore> stagedButtonScores;
  scorebot::MaintenanceAction maintenanceAction;
  uint32_t maintenanceStartedMs;
  std::atomic<uint32_t> rotaryPressStartedMs;
};

#endif // GAME_STATE_H
