# Scorebot TODO

## 🚨 Critical Issues

### Security & Stability
- [ ] **INFINITE HANG**: Add timeout to hardware initialization loop in `src/HT16Display.cpp:6-8`
- [ ] **HARDWARE TEST**: Measure connection recovery and battery life with five BLE boards.
- [ ] **HARDWARE TEST**: Exercise USB and physically armed BLE OTA updates on every board before field use.

### Thread Safety
- [ ] **ISR SAFETY**: Simplify ISR handlers in `src/ButtonGrid.cpp:5-12` and `src/RotaryEncoder.cpp:4-11`

## 📋 Backlog

### Core Features
- [ ] SOS light when idle
- [ ] IR receiver for configuration  
- [ ] Brightness control based on turn/winning status

### Code Quality Improvements
- [ ] Create typedef for node/peer ID type (`lib/scorebot/src/MyBle.hpp`)
- [ ] Implement failure modes for RTButton timer creation (`lib/scorebot/src/RTButton.hpp:43`)
- [ ] Implement failure modes for RTButton task creation (`lib/scorebot/src/RTButton.hpp:78`)
- [ ] Fix buffer size in `lib/scorebot/src/utils.hpp:8-12` (10 bytes too small)

### Type Safety & Consistency
- [ ] Standardize integer types (replace `u32_t` with `uint32_t` in `lib/scorebot/src/ButtonGrid.hpp`)
- [ ] Fix peer ID type consistency (uint32_t vs uint8_t issues noted in commits)
- [ ] Make GameState members private with const accessors (`lib/scorebot/src/GameState.hpp:13-15`)

### Architecture & Design
- [ ] Decouple GameState from Coordinator components (`src/GameState.cpp`)
- [ ] Add display state tracking to avoid redundant updates (`src/GameState.cpp`)

## 🔄 In Progress

*(Move items here when actively working on them)*

## ✅ Done

- [x] Score commitment is separate from turn passing; out-of-turn corrections remain valid.
- [x] Leaderboard ADD/Start and OK/Reset controls.

---

## 🧹 Commented-Out Code to Review

### Potentially Unfinished Features
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
