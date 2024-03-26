
Setup:

```
brew install platformio
```


Misc

```
pio run -t upload -t monitor -e leaderboard
```

```
pio run -t upload -t monitor -e player0
```

Debugging project setup and the like:

```
pio project config   --json-output
pio project metadata --json-output -e leaderboard
```

Maybe add clang-format as a convenience script?:

```sh
clang-format \
  -i \
  ./**/*.{hpp,cpp}
```

TODO: 

- Turn on light when no activity in a while. "Turn SOS"
- IR receiver to set number of players, player number, etc
- Light brightness on leaderboard indicates who's winning
- Blink to ack data sent
- Abort when no ack within timeout
- LED display less bright when not your turn
- Show self score on idle
- Don't show any scores until committed once
- Another button. Commit score != pass turn.
- Score input as a rotary encoder.
- Buttons on leaderboard?

Hardware maybes:

- Put LED on PWM-enabled pin maybe so we can dim it. Those are A0-A7.
