# Scorebot TODO

## Hardware validation

- [ ] Measure deep-sleep current and multi-day battery life on every board revision.
- [ ] Exercise recovery with the full five-board BLE topology.
- [ ] Exercise USB recovery and physically armed BLE OTA on every board before field use.

## Architecture

- [ ] Introduce a strong peer-ID type instead of passing raw `uint32_t` values.
- [ ] Move persisted and replicated state behind focused `GameState` accessors.
- [ ] Further separate pure game transitions from Coordinator-owned display and transport effects.
- [ ] Decode each wire message once into a validated tagged message instead of reparsing JSON.

## Future features

- [ ] Optional low-power idle heartbeat pattern distinct from deep-sleep status pulses.
- [ ] IR receiver for configuration.
- [ ] User-configurable brightness.
