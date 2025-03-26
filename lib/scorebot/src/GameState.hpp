#pragma once

struct GameState {
    bool isLeaderboard = false;
    int playerNumber = 0;
    volatile bool buttonPressed = false;
    volatile bool interrupted = false;
};
