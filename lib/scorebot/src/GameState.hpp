#pragma once

#include <Event.hpp>
//struct GameState {

//    bool isLeaderboard = false;
//    int playerNumber = 0;
//    volatile bool buttonPressed = false;
//    volatile bool interrupted = false;
//    // int encoderValue;
//};

class Coordinator;

class GameState {
public:
  enum class TurnState { MyTurn, OpponentTurn };
  GameState();

  void handleEvent(const Event& e, Coordinator* coordinator);

private:
  int score;
  TurnState turn;
};
