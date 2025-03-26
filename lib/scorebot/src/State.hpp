#ifndef STATE_H
#define STATE_H

#include <painlessMesh.h>
#include <GameState.hpp>
#include <set>
#include <RotaryEncoder.hpp>
#include <ButtonGrid.hpp>


struct State {
    GameState* volatile pointer;
    Scheduler userScheduler;  // Required for custom scheduled tasks
    painlessMesh mesh;
    std::set<uint32_t> peers;

    HT16Display primaryDisplay;
    HT16Display display2;
    HT16Display display3;
    HT16Display display4;
    RotaryEncoder encoder;
    ButtonGrid buttonGrid;

    explicit State(GameState* pointer)
    : pointer(pointer),
      encoder{&primaryDisplay},
      buttonGrid{&primaryDisplay}{}
};


#endif