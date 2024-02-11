
Setup:

```
brew install platformio
```


# Test Script

Terminal 1:

```
pio run -t upload -t monitor -e leaderboard
# look for 7,8,9
```

Terminal 2:

```
pio run -t upload -t monitor -e player0
# look for 0,1,2
```
