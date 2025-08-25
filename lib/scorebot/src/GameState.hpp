#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Event.hpp>
#include <list>
#include <BoardRole.hpp>

class GameState {
public:
  GameState();

  void handleEvent(const Event& e, class Coordinator* coordinator);

  int myScore;
  BoardRole whosTurn;
  std::map<BoardRole, int> scores;
  std::list<BoardRole> whosConnected;
  bool gameStarted;
};

#endif // GAME_STATE_H
