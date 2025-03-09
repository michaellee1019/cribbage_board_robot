#ifndef STATE_H
#define STATE_H

#include <set>
#include <painlessMesh.h>


struct State {
    Scheduler userScheduler;  // Required for custom scheduled tasks
    painlessMesh mesh;

    std::set<uint32_t> peers;

    bool isLeaderboard = false;
    int playerNumber = 0;
    volatile bool buttonPressed = false;
    volatile bool interrupted = false;
};


#endif