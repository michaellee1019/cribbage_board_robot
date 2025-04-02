#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Event.hpp>
#include <list>

class GameState {
public:
  GameState();

  void handleEvent(const Event& e, class Coordinator* coordinator);

  int score;
  uint32_t whosTurn;
  std::list<uint32_t> peers;
};

#endif // GAME_STATE_H
