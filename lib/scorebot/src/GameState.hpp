#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Event.hpp>
#include <set>

class GameState {
public:
  GameState();

  void handleEvent(const Event& e, class Coordinator* coordinator);

  int score;
  uint8_t whosTurn;
  std::set<uint8_t> peers;
};

#endif // GAME_STATE_H
