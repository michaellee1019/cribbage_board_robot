# Cribbage Board Robot - TODO Board

## 🚨 Critical Issues

### Security & Stability
- [ ] **CRASH RISK**: Add error handling for FreeRTOS resource creation (`xQueueCreate`, `xTaskCreate`) in `src/Coordinator.cpp:19,37`
- [ ] **MEMORY LEAK**: Fix String object lifecycle in WiFi queue (`src/MyWifi.cpp:34-56`)
- [ ] **INFINITE HANG**: Add timeout to hardware initialization loop in `src/HT16Display.cpp:6-8`

### Thread Safety
- [ ] **RACE CONDITION**: Replace volatile `ackReceived` with atomic operations in `src/MyWifi.cpp:18,23`
- [ ] **ISR SAFETY**: Simplify ISR handlers in `src/ButtonGrid.cpp:5-12` and `src/RotaryEncoder.cpp:4-11`

## 📋 Backlog

### Core Features (from CLAUDE.md)
- [ ] SOS light when idle
- [ ] IR receiver for configuration  
- [ ] Brightness control based on turn/winning status
- [ ] Score commitment vs turn passing logic
- [ ] Leaderboard buttons functionality

### Code Quality Improvements
- [ ] Create typedef for node/peer ID type (`src/MyWifi.cpp:22`)
- [ ] Implement failure modes for RTButton timer creation (`lib/scorebot/src/RTButton.hpp:43`)
- [ ] Implement failure modes for RTButton task creation (`lib/scorebot/src/RTButton.hpp:78`)
- [ ] Add exception handling for `std::stoi()` in `src/GameState.cpp:37`
- [ ] Fix buffer size in `lib/scorebot/src/utils.hpp:8-12` (10 bytes too small)

### Type Safety & Consistency
- [ ] Standardize integer types (replace `u32_t` with `uint32_t` in `lib/scorebot/src/ButtonGrid.hpp`)
- [ ] Fix peer ID type consistency (uint32_t vs uint8_t issues noted in commits)
- [ ] Make GameState members private with const accessors (`lib/scorebot/src/GameState.hpp:13-15`)

### Architecture & Design
- [ ] Extract magic numbers to named constants (`src/MyWifi.cpp:10-12,19,67`)
- [ ] Implement proper RAII or cleanup for FreeRTOS resources (`src/MyWifi.cpp:17-18`)
- [ ] Decouple GameState from Coordinator components (`src/GameState.cpp:63-75`)
- [ ] Add display state tracking to avoid redundant updates (`src/GameState.cpp:64-74`)

## 🔄 In Progress

*(Move items here when actively working on them)*

## ✅ Done

*(Completed items will be moved here)*

---

## 🧹 Commented-Out Code to Review

### Potentially Unfinished Features
- [ ] **WiFi Retry Logic**: Review commented-out retry/ack system in `src/MyWifi.cpp:42-54`
- [ ] **Event Visitor Pattern**: Evaluate commented-out EventVisitor implementation in `lib/scorebot/src/Event.hpp:38-54`
- [ ] **Input Handling**: Review commented-out encoder/button code in `src/GameState.cpp:19-20`

### Library Dependencies
- [ ] **Unused Libraries**: Clean up commented library deps in `platformio.ini:10-27`

## 🐛 Known Bugs

- [ ] **Fade Logic Bug**: "may have a subtle bug idk sometimes the light stays on" (commit 5e7479a)
- [ ] **Untested Changes**: Review changes from commit 3ab7817 "remove some cruft (changes untested)"

## 📚 Code Patterns to Establish

### Error Handling
- [ ] Establish consistent error handling patterns across the codebase
- [ ] Add timeout patterns for hardware operations
- [ ] Implement proper exception handling for network operations

### Memory Management  
- [ ] Establish RAII patterns for FreeRTOS resources
- [ ] Review all dynamic allocations for proper cleanup
- [ ] Implement safe string handling patterns

### Concurrency
- [ ] Establish ISR safety guidelines
- [ ] Review all shared state for thread safety
- [ ] Document synchronization requirements

---

*This TODO board follows a kanban-style workflow. Move items between sections as work progresses. Priority should be given to Critical Issues first, then Core Features.*