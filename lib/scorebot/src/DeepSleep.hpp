#pragma once

class Coordinator;

namespace scorebot::deep_sleep {

// Timer wakes only run the minimal status-pulse path and return to deep sleep.
// Physical GPIO wakes continue through normal setup and BLE rejoining.
void handleTimerWake(Coordinator& coordinator);
[[noreturn]] void enter(Coordinator& coordinator);

}  // namespace scorebot::deep_sleep
