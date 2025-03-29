#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Event.hpp>

#include <set>
#include <WString.h>

class GameState {
public:
  enum class TurnState { MyTurn, OpponentTurn };
  GameState();

  void handleEvent(const Event& e, class Coordinator* coordinator);

private:
  int score;
  TurnState turn;
  std::set<uint8_t> peers;
};

#endif // GAME_STATE_H
