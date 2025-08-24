#include <Arduino.h>
#include <set>

#include <SparkFun_Alphanumeric_Display.h>

#include <Adafruit_MCP23X17.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

#include <WiFi.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_wifi.h>
#include <BoardTypes.hpp>


#define MESH_PREFIX "mesh_network"
#define MESH_PASSWORD "mesh_password"
#define MESH_PORT 5555

// Message structure for player updates
struct PlayerMessage {
    int32_t score;
    bool turnPassed;
    uint32_t fromNodeId;
    
    // Constructor
    PlayerMessage(int32_t s = 0, bool t = false, uint32_t nodeId = 0) 
        : score(s), turnPassed(t), fromNodeId(nodeId) {
        Serial.printf("DEBUG PlayerMessage created: score=%d, turnPassed=%s, fromNodeId=%u\n", 
                     s, t ? "true" : "false", nodeId);
    }
    
    // Serialize to JSON string
    String toJson() const {
        JsonDocument doc;
        doc["type"] = "player";
        doc["score"] = score;
        doc["turnPassed"] = turnPassed;
        doc["fromNodeId"] = fromNodeId;
        
        String output;
        serializeJson(doc, output);
        return output;
    }
    
    // Deserialize from JSON string
    static PlayerMessage fromJson(const String& jsonStr) {
        JsonDocument doc;
        deserializeJson(doc, jsonStr);
        
        return PlayerMessage(
            doc["score"] | 0,
            doc["turnPassed"] | false,
            doc["fromNodeId"] | 0
        );
    }
    
    // Validation updated for signed integers
    static bool isPlayerMessage(const String& jsonStr) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        return error == DeserializationError::Ok && 
               doc["type"].is<const char*>() && 
               doc["type"] == "player" &&
               doc["score"].is<int>() &&
               doc["turnPassed"].is<bool>() &&
               doc["fromNodeId"].is<unsigned int>();
    }
};

// Remove HeartbeatMessage struct and replace with ConnectionMessage
struct ConnectionMessage {
    uint32_t nodeId;
    BoardRole role;
    String roleName;
    bool isConnected;  // true for new connection, false for disconnection
    
    // Constructor
    ConnectionMessage(uint32_t id = 0, BoardRole r = BoardRole::Leader, const String& name = "", bool connected = true) 
        : nodeId(id), role(r), roleName(name), isConnected(connected) {}
    
    // Serialize to JSON string
    String toJson() const {
        JsonDocument doc;
        doc["type"] = "connection";
        doc["nodeId"] = nodeId;
        doc["role"] = (int)role;
        doc["roleName"] = roleName;
        doc["isConnected"] = isConnected;
        
        String output;
        serializeJson(doc, output);
        return output;
    }
    
    // Deserialize from JSON string
    static ConnectionMessage fromJson(const String& jsonStr) {
        JsonDocument doc;
        deserializeJson(doc, jsonStr);
        
        return ConnectionMessage(
            doc["nodeId"] | 0,
            (BoardRole)(doc["role"] | 0),
            doc["roleName"] | String(""),
            doc["isConnected"] | true
        );
    }
    
    // Check if JSON string is a valid ConnectionMessage
    static bool isConnectionMessage(const String& jsonStr) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        return error == DeserializationError::Ok && 
               doc["type"].is<const char*>() && 
               doc["type"] == "connection" &&
               doc["nodeId"].is<unsigned int>() && 
               doc["role"].is<int>();
    }
};

// Message structure for turn notifications from leader
struct TurnMessage {
    uint32_t nextPlayerNodeId;
    String nextPlayerName;
    
    // Constructor
    TurnMessage(uint32_t nodeId = 0, const String& name = "") 
        : nextPlayerNodeId(nodeId), nextPlayerName(name) {}
    
    // Serialize to JSON string
    String toJson() const {
        JsonDocument doc;
        doc["type"] = "turn";
        doc["nextPlayerNodeId"] = nextPlayerNodeId;
        doc["nextPlayerName"] = nextPlayerName;
        
        String output;
        serializeJson(doc, output);
        return output;
    }
    
    // Enhanced TurnMessage fromJson with detailed debugging
    static TurnMessage fromJson(const String& jsonStr) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        
        Serial.printf("DEBUG TurnMessage JSON: %s\n", jsonStr.c_str());
        Serial.printf("DEBUG Parse error: %s\n", error.c_str());
        
        // Check if the field exists and what type it is
        if (!doc["nextPlayerNodeId"].isNull()) {
            Serial.printf("DEBUG nextPlayerNodeId exists\n");
            if (doc["nextPlayerNodeId"].is<uint32_t>()) {
                Serial.printf("DEBUG nextPlayerNodeId is uint32_t\n");
            } else if (doc["nextPlayerNodeId"].is<int>()) {
                Serial.printf("DEBUG nextPlayerNodeId is int\n");
            } else if (doc["nextPlayerNodeId"].is<const char*>()) {
                Serial.printf("DEBUG nextPlayerNodeId is string\n");
            }
            Serial.printf("DEBUG Raw value: %s\n", doc["nextPlayerNodeId"].as<String>().c_str());
        } else {
            Serial.printf("DEBUG nextPlayerNodeId field missing!\n");
        }
        
        uint32_t nodeId = doc["nextPlayerNodeId"].as<uint32_t>();  // Use .as<uint32_t>() instead of |
        String playerName = doc["nextPlayerName"] | String("");
        
        Serial.printf("DEBUG TurnMessage parse result: nodeId=%u, name=%s\n", nodeId, playerName.c_str());
        
        return TurnMessage(nodeId, playerName);
    }
    
    // Check if JSON string is a valid TurnMessage
    static bool isTurnMessage(const String& jsonStr) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        return error == DeserializationError::Ok && 
               doc["type"].is<const char*>() && 
               doc["type"] == "turn" &&
               doc["nextPlayerNodeId"].is<unsigned int>();  // More specific type check
    }
};

Scheduler userScheduler;  // Required for custom scheduled tasks
painlessMesh mesh;

std::set<uint32_t> peers;

// Simple node mapping - nodeId directly maps to role
std::map<uint32_t, BoardRole> nodeRoleMap;

BoardRole myRole;
std::optional<BoardRoleConfig> myRoleConfig;
int32_t currentScore;

// Forward declarations
void updateLeaderDisplays();
BoardRole findNextAvailablePlayer(BoardRole startFrom);
uint32_t getNodeIdForRole(BoardRole role);
void updateDisplayBrightness();  // Add this line

// Leaderboard structure to store player statistics
struct PlayerStats {
    int32_t totalScore = 0;
    int32_t lastOffsetScore = 0;  // Points scored in the last turn
    String playerName = "";
    bool isConnected = false;        // Whether player is currently connected
    
    PlayerStats() = default;
    PlayerStats(const String& name) : playerName(name), isConnected(true) {}
};

// Leaderboard storage - maps nodeId to player stats
std::map<uint32_t, PlayerStats> leaderboard;

// Game state
bool gameStarted = false;

// Turn management
BoardRole currentTurn = BoardRole::Player_Red; // Game starts with RED

String formatMacAddress(uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

// Fix 1: Update getNodeRole to handle nodeId 0 better
BoardRole getNodeRole(uint32_t nodeId) {
    // Ignore invalid nodeId
    if (nodeId == 0) {
        Serial.printf("DEBUG: Invalid nodeId 0 ignored\n");
        return BoardRole::Leader; // Still return something, but don't log as unknown
    }
    
    // Known nodeId mappings based on actual MAC addresses
    // Leader: 98:3d:ae:ea:17:bc -> nodeId from bytes 2-5: ae:ea:17:bc = 2934577084
    // Red:    30:30:f9:33:e9:78 -> nodeId from bytes 2-5: f9:33:e9:78 = 4180928888
    // Blue:   64:e8:33:50:c4:b8 -> nodeId from bytes 2-5: 33:50:c4:b8 = 855277752
    // Green:  TBD -> nodeId from bytes 2-5: TBD
    // White:  TBD -> nodeId from bytes 2-5: TBD
    
    switch(nodeId) {
        case 2934577084: // ae:ea:17:bc from 98:3d:ae:ea:17:bc
            return BoardRole::Leader;
        case 2934574912: // f9:33:e9:78 from 30:30:f9:33:e9:78  
            return BoardRole::Player_Red;
        case 860931256: // 33:50:c4:b8 from 64:e8:33:50:c4:b8
            return BoardRole::Player_Blue;
        case 2934416992: // e7:a6:60 from 98:3d:ae:e7:a6:60
            return BoardRole::Player_White;
        case 2934574676: // 0e:54 from 98:3d:ae:ea:0e:54
            return BoardRole::Player_Green;
        default:
            Serial.printf("DEBUG: Unknown nodeId %u, defaulting to Leader\n", nodeId);
            return BoardRole::Leader;
    }
}

String getNodeName(uint32_t nodeId) {
    BoardRole role = getNodeRole(nodeId);
    auto config = getMyRoleConfig(role);
    return config.has_value() ? config->name.c_str() : "Unknown";
}

// Fix 2: Update getNextTurn to ensure proper rotation
BoardRole getNextTurn(BoardRole currentRole) {
    // Turn order: RED -> BLUE -> GREEN -> WHITE -> RED (loops)
    // But skip players that aren't connected
    BoardRole nextRole;
    switch(currentRole) {
        case BoardRole::Player_Red:
            nextRole = BoardRole::Player_Blue;
            break;
        case BoardRole::Player_Blue:
            nextRole = BoardRole::Player_Green;
            break;
        case BoardRole::Player_Green:
            nextRole = BoardRole::Player_White;
            break;
        case BoardRole::Player_White:
            nextRole = BoardRole::Player_Red;
            break;
        default:
            nextRole = BoardRole::Player_Red; // Default to RED
            break;
    }
    
    // Check if next player is connected, if not, keep looking
    uint32_t nextNodeId = getNodeIdForRole(nextRole);
    if (nextNodeId != 0 && leaderboard.find(nextNodeId) != leaderboard.end() && leaderboard[nextNodeId].isConnected) {
        return nextRole;
    } else {
        // Recursively find next available player
        return getNextTurn(nextRole);
    }
}

uint32_t getNodeIdForRole(BoardRole role) {
    // Find nodeId for a given role in our connected nodes
    for (auto& pair : nodeRoleMap) {
        if (pair.second == role) {
            return pair.first;
        }
    }
    return 0; // Role not found in connected nodes
}

// Fix 3: Update broadcastNextTurn to ensure proper turn management
void broadcastNextTurn() {
    // Only leader should call this
    if (myRoleConfig->role != BoardRole::Leader) return;
    
    Serial.printf("DEBUG: Current turn before change: %d (%s)\n", 
                 (int)currentTurn, getNodeName(getNodeIdForRole(currentTurn)).c_str());
    
    BoardRole nextRole = getNextTurn(currentTurn);
    uint32_t nextNodeId = getNodeIdForRole(nextRole);
    
    Serial.printf("DEBUG: Next turn calculated: %d (%s), NodeID: %u\n", 
                 (int)nextRole, getNodeName(nextNodeId).c_str(), nextNodeId);
    
    if (nextNodeId != 0) {
        currentTurn = nextRole; // Update current turn
        String nextPlayerName = getNodeName(nextNodeId);
        
        TurnMessage turnMsg(nextNodeId, nextPlayerName);
        String jsonStr = turnMsg.toJson();
        mesh.sendBroadcast(jsonStr);
        
        Serial.printf("Broadcasting next turn: %s (NodeID: %u)\n", 
                     nextPlayerName.c_str(), nextNodeId);
        
        // Update brightness for leader displays
        updateDisplayBrightness();
    } else {
        Serial.printf("Warning: Next player role %d not found in connected nodes\n", (int)nextRole);
    }
}

void readMacAddress(uint8_t* macOut){
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, macOut);
    if (ret == ESP_OK) {
      Serial.printf("my mac addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
                    macOut[0], macOut[1], macOut[2],
                    macOut[3], macOut[4], macOut[5]);
    } else {
      Serial.println("Failed to read MAC address");
    }
}
  
template <typename... Args>
String strFormat(const char* const format, Args... args) {
    char buffer[10];
    std::snprintf(buffer, sizeof(buffer), format, args...);
    return {buffer};
}

// Fix 1: Update player display initialization to show "----" when game starts
class HT16Display {
    HT16K33 driver;

public:
    HT16Display() = default;
    void setup(uint8_t address) {
        while (!driver.begin(address)) {
        }
        // Set default brightness to 75%
        setBrightness(75);
    }

    // Enhanced setBrightness with debugging
    void setBrightness(uint8_t brightness) {
        // HT16K33 brightness range is 0-15, convert percentage to this range
        uint8_t brightnessLevel = map(brightness, 0, 100, 0, 15);
        driver.setBrightness(brightnessLevel);
        Serial.printf("DEBUG: Set brightness to %u%% (level %u)\n", brightness, brightnessLevel);
    }

    // Talks like a duck!
    template <typename... Args>
    auto print(Args&&... args) {
        return driver.print(std::forward<Args>(args)...);
    }
};

volatile bool buttonPressed = false;
static void buttonISR() {
    buttonPressed = true;
}


// Variables and functions needed by RotaryEncoder
volatile bool interrupted = false;
static void seesawInterrupt() {
    interrupted = true;
}

// RotaryEncoder class definition moved here
class RotaryEncoder {
#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SEESAW_ADDR 0x36
#define SEESAW_INTERRUPT 7

    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

    HT16Display* const display;

public:
    explicit RotaryEncoder(HT16Display* const display) : display{display} {}

    void setup() {

        ss.begin(SEESAW_ADDR);
        sspixel.begin(SEESAW_ADDR);
        sspixel.setPixelColor(0, 0x000000);
        // sspixel.setBrightness(1);
        // sspixel.setPixelColor(0, 0xFAEDED);
        sspixel.show();

        // https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
        ss.pinMode(SS_SWITCH, INPUT_PULLUP);

        static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

        pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
        ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
        ss.setGPIOInterrupts(mask, true);
        ss.enableEncoderInterrupt();

        attachInterrupt(digitalPinToInterrupt(SEESAW_INTERRUPT), seesawInterrupt, CHANGE);
    }

    void reset() {
        ss.setEncoderPosition(0);
    }

    void loop() {
       if (interrupted) {
        int32_t position = ss.getEncoderPosition();
        display->print(strFormat("%d", position));
        currentScore = position;
        interrupted = false;
       }
    }
};

class ButtonGrid {
    HT16Display* const display;
    Adafruit_MCP23X17 buttonGpio;
    RotaryEncoder* encoder;  // Change to pointer

    static constexpr u32_t interruptPin = 8;

    static constexpr u32_t okPin = 4;
    static constexpr u32_t plusonePin = 3;
    static constexpr u32_t plusfivePin = 2;
    static constexpr u32_t negonePin = 1;
    static constexpr u32_t addPin = 0;

    static constexpr u32_t intValPressed = 17;
    static constexpr u32_t intValReleased = 31;

    static constexpr auto pins = {okPin, plusonePin, plusfivePin, negonePin, addPin};

public:
    explicit ButtonGrid(HT16Display* const display, RotaryEncoder* encoder) : display{display}, encoder{encoder} {}

    void setup() {
        buttonGpio.begin_I2C(0x20, &Wire);
        buttonGpio.setupInterrupts(true, false, LOW);
        for (auto&& pin : pins) {
            buttonGpio.pinMode(pin, INPUT_PULLUP);
            buttonGpio.setupInterruptPin(pin, CHANGE);
        }
        pinMode(interruptPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(interruptPin), buttonISR, CHANGE);
        buttonGpio.clearInterrupts();
    }

    // Fix 1: Update button handling to not reset score on ADD, and allow out-of-turn scoring
    void loop() {
        if (!buttonPressed) {
            return;
        }

        uint8_t intPin = buttonGpio.getLastInterruptPin();
        uint8_t intVal = buttonGpio.getCapturedInterrupt();
        buttonPressed = false;
        buttonGpio.clearInterrupts();

        Serial.printf("DEBUG: Button pressed: intPin=%d, intVal=%d\n", intPin, intVal);

        if (intVal == intValReleased) {
            // Handle leader board buttons (only 4 buttons: pins 0-3)
            if (myRoleConfig->role == BoardRole::Leader) {
                if (intPin == 0 && !gameStarted) {  // Button 0 starts the game
                    startGame();
                    return;
                }
                // Other leader buttons can be used for other functions later
            }
            // Handle player board buttons
            else {
                if (intPin == addPin) {
                    // ADD button: Submit current score without passing turn (can be done anytime)
                    if (currentScore != 0) {
                        uint32_t myNodeId = mesh.getNodeId();
                        PlayerMessage msg(currentScore, false, myNodeId);  // Pass actual nodeId
                        String jsonStr = msg.toJson();
                        mesh.sendBroadcast(jsonStr);
                        Serial.printf("Sent score update (no turn pass): %s\n", jsonStr.c_str());
                        
                        // Reset current score to 0 after submission
                        currentScore = 0;
                        encoder->reset();
                        
                        // Keep BEEF displayed if it's still my turn
                        if (gameStarted && getNodeIdForRole(currentTurn) == myNodeId) {
                            display->print("BEEF");
                            Serial.printf("Keeping BEEF display - still my turn after ADD\n");
                        }
                    } else {
                        Serial.printf("No score to add\n");
                    }
                } else if (intPin == negonePin) {
                    currentScore--;
                } else if (intPin == plusonePin) {
                    currentScore++;
                } else if (intPin == plusfivePin) {
                    currentScore += 5;
                } else if (intPin == okPin) {
                    // OK button: Submit current score AND pass turn
                    uint32_t myNodeId = mesh.getNodeId();
                    PlayerMessage msg(currentScore, true, myNodeId);  // Pass actual nodeId
                    String jsonStr = msg.toJson();
                    mesh.sendBroadcast(jsonStr);
                    Serial.printf("Sent score update with turn pass: %s\n", jsonStr.c_str());
                    
                    // Reset current score to 0 after submission
                    currentScore = 0;
                    encoder->reset();
                }
            }

            // Display logic for player boards - but don't overwrite BEEF!
            if (myRoleConfig->role != BoardRole::Leader) {
                // Only update display if we're building a score or have reset to 0
                // Don't overwrite BEEF or other turn indicators
                if (intPin == negonePin || intPin == plusonePin || intPin == plusfivePin || 
                    (intPin == addPin && currentScore == 0) || (intPin == okPin && currentScore == 0)) {
                    
                    // Display "----" when currentScore is 0, otherwise show the score (including negative)
                    if (currentScore == 0) {
                        display->print("----");
                        Serial.printf("Display updated to ---- for %s (reason: score=0)\n", myRoleConfig->name.c_str());
                    } else {
                        display->print(strFormat("%d", currentScore));  // Will correctly show negative numbers
                        Serial.printf("Display updated to %d for %s (reason: building score)\n", currentScore, myRoleConfig->name.c_str());
                    }
                } else {
                    Serial.printf("Display NOT updated for %s (preserving current display)\n", myRoleConfig->name.c_str());
                }
            }
        }
    }

private:
    void startGame() {
        gameStarted = true;
        Serial.println("=== GAME STARTED ===");
        
        // Debug: Print current nodeRoleMap
        Serial.println("=== DEBUG: Current nodeRoleMap ===");
        for (auto& pair : nodeRoleMap) {
            Serial.printf("NodeID: %u -> Role: %d (%s)\n", 
                         pair.first, (int)pair.second, getNodeName(pair.first).c_str());
        }
        
        // Debug: Print current leaderboard
        Serial.println("=== DEBUG: Current leaderboard ===");
        for (auto& pair : leaderboard) {
            Serial.printf("NodeID: %u -> Name: %s, Connected: %s\n", 
                         pair.first, pair.second.playerName.c_str(), 
                         pair.second.isConnected ? "YES" : "NO");
        }
        
        // Count connected PLAYERS only (exclude Leaders)
        int connectedPlayerCount = 0;
        uint32_t redNodeId = 0;
        bool redAvailable = false;
        
        for (auto& pair : leaderboard) {
            if (pair.second.isConnected) {
                connectedPlayerCount++;
                Serial.printf("Player in game: %s (NodeID: %u)\n", pair.second.playerName.c_str(), pair.first);
                
                // Check if this is RED
                if (getNodeRole(pair.first) == BoardRole::Player_Red) {
                    redNodeId = pair.first;
                    redAvailable = true;
                    Serial.printf("Found RED player: NodeID %u\n", redNodeId);
                }
            }
        }
        
        Serial.printf("Starting game with %d players\n", connectedPlayerCount);
        
        // Update all displays to show current scores
        updateLeaderDisplays();
        
        // Force RED to start if available
        if (redAvailable && redNodeId != 0) {
            currentTurn = BoardRole::Player_Red;
            Serial.printf("Game starting with RED player (NodeID: %u) - FORCED\n", redNodeId);
            
            // Send turn message to RED directly (don't call broadcastNextTurn!)
            String redPlayerName = getNodeName(redNodeId);
            TurnMessage turnMsg(redNodeId, redPlayerName);
            String jsonStr = turnMsg.toJson();
            mesh.sendBroadcast(jsonStr);
            Serial.printf("Broadcasting FIRST turn directly to RED: %s (NodeID: %u)\n", 
                         redPlayerName.c_str(), redNodeId);
        } else {
            // RED not available, find first available player
            Serial.printf("RED not available, finding first available player\n");
            currentTurn = findNextAvailablePlayer(BoardRole::Player_Red);
            uint32_t firstNodeId = getNodeIdForRole(currentTurn);
            
            if (currentTurn != BoardRole::Leader && firstNodeId != 0) {
                Serial.printf("First available player: %d (%s, NodeID: %u)\n", 
                             (int)currentTurn, getNodeName(firstNodeId).c_str(), firstNodeId);
                
                // Send turn message to first player directly
                String firstPlayerName = getNodeName(firstNodeId);
                TurnMessage turnMsg(firstNodeId, firstPlayerName);
                String jsonStr = turnMsg.toJson();
                mesh.sendBroadcast(jsonStr);
                Serial.printf("Broadcasting FIRST turn directly to: %s (NodeID: %u)\n", 
                             firstPlayerName.c_str(), firstNodeId);
            }
        }
        
        // Update display brightness after game starts
        updateDisplayBrightness();
    }
    
    BoardRole findNextAvailablePlayer(BoardRole startFrom) {
        BoardRole roles[] = {BoardRole::Player_Red, BoardRole::Player_Blue, BoardRole::Player_Green, BoardRole::Player_White};
        int startIndex = 0;
        
        // Find starting position
        for (int i = 0; i < 4; i++) {
            if (roles[i] == startFrom) {
                startIndex = i;
                break;
            }
        }
        
        // Look for next available player
        for (int i = 0; i < 4; i++) {
            int index = (startIndex + i) % 4;
            uint32_t nodeId = getNodeIdForRole(roles[index]);
            if (nodeId != 0 && leaderboard.find(nodeId) != leaderboard.end() && leaderboard[nodeId].isConnected) {
                return roles[index];
            }
        }
        
        return BoardRole::Leader; // No players available
    }
};

class RTButton {
public:
    RTButton(int pin, int debounceDelay, int doubleClickThreshold)
        : buttonPin(pin),
          debounceDelayMs(debounceDelay),
          doubleClickThresholdMs(doubleClickThreshold),
          lastInterruptTime(0) {
        buttonSemaphore = xSemaphoreCreateBinary();

        debounceTimer = xTimerCreate("Debounce Timer",
                                     pdMS_TO_TICKS(debounceDelayMs),
                                     pdFALSE,  // One-shot timer
                                     this,
                                     [](TimerHandle_t xTimer) {
                                         auto* button =
                                             static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                                         if (button)
                                             button->debounceCallback();
                                     });

        doubleClickTimer = xTimerCreate("Double-Click Timer",
                                        pdMS_TO_TICKS(doubleClickThresholdMs),
                                        pdFALSE,  // One-shot timer
                                        this,
                                        [](TimerHandle_t xTimer) {
                                            auto* button =
                                                static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                                            if (button)
                                                button->doubleClickCallback();
                                        });

        if (debounceTimer == nullptr || doubleClickTimer == nullptr) {
            Serial.println("Failed to create timers");
            // TODO: have a failure mode
        }
    }


    virtual ~RTButton() {
        if (debounceTimer != nullptr) {
            xTimerDelete(debounceTimer, portMAX_DELAY);
        }
        if (doubleClickTimer != nullptr) {
            xTimerDelete(doubleClickTimer, portMAX_DELAY);
        }
        if (buttonSemaphore != nullptr) {
            vSemaphoreDelete(buttonSemaphore);
        }
    }

    void init() {
        pinMode(buttonPin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(buttonPin), ISRHandler, this, CHANGE);

        BaseType_t taskCreated = xTaskCreate(
            [](void* param) {
                auto* button = static_cast<RTButton*>(param);
                if (button)
                    button->buttonTask();
            },
            "Button Task",
            2048,
            this,
            1,
            nullptr);

        if (taskCreated != pdPASS) {
            Serial.println("Failed to create button task");
            // TODO: handle failure modes
        }
    }

protected:
    virtual void onPress() {
        Serial.println("Button Pressed");
    }

    virtual void onRelease() {
        Serial.println("Button Released");
    }

    virtual void onSingleClick() {
        Serial.println("Single Click Detected");
    }

    virtual void onDoubleClick() {
        Serial.println("Double Click Detected");
    }

private:
    const unsigned short buttonPin;
    const unsigned short debounceDelayMs;
    const unsigned short doubleClickThresholdMs;

    SemaphoreHandle_t buttonSemaphore;
    TimerHandle_t debounceTimer;
    TimerHandle_t doubleClickTimer;

    volatile bool buttonPressed = false;
    volatile bool isDoubleClick = false;
    volatile size_t clickCount = 0;
    volatile TickType_t lastInterruptTime = 0;

    static void IRAM_ATTR ISRHandler(void* arg) {
        auto* button = static_cast<RTButton*>(arg);
        if (button)
            button->handleInterrupt();
    }

    void handleInterrupt() {
        TickType_t interruptTime = xTaskGetTickCountFromISR();

        if ((interruptTime - lastInterruptTime) * portTICK_PERIOD_MS > debounceDelayMs) {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xTimerResetFromISR(debounceTimer, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        lastInterruptTime = interruptTime;
    }

    void debounceCallback() {
        if (digitalRead(buttonPin) == LOW) {
            buttonPressed = true;
            xSemaphoreGive(buttonSemaphore);
        } else {
            buttonPressed = false;
            xSemaphoreGive(buttonSemaphore);
        }
    }

    void doubleClickCallback() {
        if (clickCount == 1) {
            onSingleClick();
        } else if (clickCount == 2) {
            isDoubleClick = true;
            onDoubleClick();
        }
        clickCount = 0;
    }

    [[noreturn]]
    void buttonTask() {
        while (true) {
            if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
                if (buttonPressed) {
                    onPress();
                    clickCount++;
                    xTimerStart(doubleClickTimer, 0);
                } else {
                    onRelease();
                }
            }
        }
    }
};

HT16Display primaryDisplay;
HT16Display display2;
HT16Display display3;
HT16Display display4;

RotaryEncoder encoder{&primaryDisplay};
ButtonGrid buttonGrid(&primaryDisplay, &encoder);

// Function to broadcast connection status to other nodes
void broadcastConnectionStatus(uint32_t nodeId, BoardRole role, const String& roleName, bool isConnected) {
    ConnectionMessage connMsg(nodeId, role, roleName, isConnected);
    String jsonStr = connMsg.toJson();
    mesh.sendBroadcast(jsonStr);
    Serial.printf("Broadcasting connection status: %s (NodeID: %u, Connected: %s)\n", 
                 roleName.c_str(), nodeId, isConnected ? "YES" : "NO");
}

// Enhanced updateLeaderDisplays to better handle disconnected players
void updateLeaderDisplays() {
    if (myRoleConfig->role != BoardRole::Leader) return;
    
    // Track what each display should show
    String redDisplay = "";
    String blueDisplay = "";
    String greenDisplay = "";
    String whiteDisplay = "";
    
    // Determine the correct display text for each player (connected or not)
    for (auto& pair : leaderboard) {
        uint32_t nodeId = pair.first;
        PlayerStats& stats = pair.second;
        BoardRole playerRole = getNodeRole(nodeId);
        
        String displayText;
        if (stats.isConnected) {
            if (gameStarted) {
                // During game, show scores (including 0 and negative)
                displayText = strFormat("%d", stats.totalScore);
            } else {
                // Before game starts, show player role names
                displayText = stats.playerName;
            }
        } else {
            // Player is disconnected
            if (gameStarted) {
                // During game, hide disconnected players (empty string)
                displayText = "";
            } else {
                // Before game, show "----" for disconnected
                displayText = "----";
            }
        }
        
        // Set the appropriate display variable
        switch (playerRole) {
            case BoardRole::Player_Red:
                redDisplay = displayText;
                break;
            case BoardRole::Player_Blue:
                blueDisplay = displayText;
                break;
            case BoardRole::Player_Green:
                greenDisplay = displayText;
                break;
            case BoardRole::Player_White:
                whiteDisplay = displayText;
                break;
            default:
                break;
        }
    }
    
    // Set defaults for unconnected players
    if (redDisplay == "") redDisplay = gameStarted ? "" : "----";
    if (blueDisplay == "") blueDisplay = gameStarted ? "" : "----";
    if (greenDisplay == "") greenDisplay = gameStarted ? "" : "----";
    if (whiteDisplay == "") whiteDisplay = gameStarted ? "" : "----";
    
    // Update all displays at once
    primaryDisplay.print(redDisplay);
    display2.print(blueDisplay);
    display3.print(greenDisplay);
    display4.print(whiteDisplay);
    
    Serial.printf("Updated displays: RED=%s, BLUE=%s, GREEN=%s, WHITE=%s\n", 
                 redDisplay.c_str(), blueDisplay.c_str(), greenDisplay.c_str(), whiteDisplay.c_str());
}

// Fix 1: Remove Leader from leaderboard in connection handling
void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("*** NEW CONNECTION CALLBACK: nodeId=%u ***\n", nodeId);
    
    // Ignore invalid nodeIds
    if (nodeId == 0) {
        Serial.printf("Ignoring invalid nodeId 0\n");
        return;
    }
    
    // Don't process our own nodeId
    if (nodeId == mesh.getNodeId()) {
        Serial.printf("Ignoring self-connection\n");
        return;
    }
    
    peers.emplace(nodeId);
    
    // Automatically determine role from nodeId
    BoardRole nodeRole = getNodeRole(nodeId);
    String nodeName = getNodeName(nodeId);
    nodeRoleMap[nodeId] = nodeRole;
    
    Serial.printf("Connected node %u identified as: %s (role=%d)\n", 
                 nodeId, nodeName.c_str(), (int)nodeRole);
    
    // Update leaderboard ONLY if this is a PLAYER (not Leader)
    if (myRoleConfig->role == BoardRole::Leader && nodeRole != BoardRole::Leader) {
        if (leaderboard.find(nodeId) == leaderboard.end()) {
            leaderboard[nodeId] = PlayerStats(nodeName);
        }
        leaderboard[nodeId].isConnected = true;
        updateLeaderDisplays();
    }
    
    // Broadcast this connection to other nodes (echo system)
    broadcastConnectionStatus(nodeId, nodeRole, nodeName, true);
    
    // Print all known nodes
    Serial.println("=== All Connected Nodes ===");
    for (auto& pair : nodeRoleMap) {
        uint32_t id = pair.first;
        BoardRole role = pair.second;
        String name = getNodeName(id);
        Serial.printf("NodeID: %u, Role: %s (%d)\n", id, name.c_str(), (int)role);
    }
    Serial.println("===========================");
}

// Enhanced lostConnectionCallback with better disconnect handling
void lostConnectionCallback(uint32_t nodeId) {
    Serial.printf("*** LOST CONNECTION CALLBACK: nodeId=%u ***\n", nodeId);
    
    // Ignore invalid nodeIds
    if (nodeId == 0) {
        Serial.printf("Ignoring invalid nodeId 0 in disconnect\n");
        return;
    }
    
    // Don't process our own nodeId
    if (nodeId == mesh.getNodeId()) {
        Serial.printf("Ignoring self-disconnect\n");
        return;
    }
    
    peers.erase(nodeId);
    
    BoardRole nodeRole = getNodeRole(nodeId);
    String nodeName = getNodeName(nodeId);
    
    Serial.printf("Disconnecting node %u identified as: %s (role=%d)\n", 
                 nodeId, nodeName.c_str(), (int)nodeRole);
    
    // Update leaderboard if this is the leader
    if (myRoleConfig->role == BoardRole::Leader) {
        if (leaderboard.find(nodeId) != leaderboard.end()) {
            leaderboard[nodeId].isConnected = false;
            Serial.printf("Marked %s as disconnected in leaderboard\n", nodeName.c_str());
            updateLeaderDisplays();
        } else {
            Serial.printf("NodeID %u not found in leaderboard\n", nodeId);
        }
    }
    
    // Broadcast this disconnection to other nodes (echo system)
    broadcastConnectionStatus(nodeId, nodeRole, nodeName, false);
    
    // Remove from nodeRoleMap
    nodeRoleMap.erase(nodeId);
    Serial.printf("Removed %s (NodeID: %u) from nodeRoleMap\n", nodeName.c_str(), nodeId);
    
    // Print updated node list
    Serial.println("=== Updated Connected Nodes ===");
    for (auto& pair : nodeRoleMap) {
        uint32_t id = pair.first;
        BoardRole role = pair.second;
        String name = getNodeName(id);
        Serial.printf("NodeID: %u, Role: %s (%d)\n", id, name.c_str(), (int)role);
    }
    Serial.println("===============================");
}

void changedConnectionsCallback() {
    Serial.println("*** CHANGED CONNECTIONS CALLBACK ***");
    Serial.printf("Current connections: %d\n", peers.size());
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("*** NODE TIME ADJUSTED: offset=%d ***\n", offset);
}

// Fix 2: Clean up connectionMessage handling to exclude Leaders
void receivedCallback(uint32_t from, String& msg) {
    String fromName = getNodeName(from);
    Serial.printf("*** RECEIVED MESSAGE: [%s] from %u (%s) ***\n", 
                 msg.c_str(), from, fromName.c_str());
    
    // Ignore messages from invalid nodeIds
    if (from == 0) {
        Serial.printf("Ignoring message from invalid nodeId 0\n");
        return;
    }
    
    // Try to parse as ConnectionMessage first
    if (ConnectionMessage::isConnectionMessage(msg)) {
        ConnectionMessage connMsg = ConnectionMessage::fromJson(msg);
        
        // Don't process our own connection messages
        if (connMsg.nodeId == mesh.getNodeId()) {
            Serial.printf("Ignoring self-connection message\n");
            return;
        }
        
        Serial.printf("Received connection message: %s (NodeID: %u, Connected: %s)\n", 
                     connMsg.roleName.c_str(), connMsg.nodeId, connMsg.isConnected ? "YES" : "NO");
        
        // Update leaderboard (only leader processes connection messages for display)
        // AND only for PLAYERS (not other Leaders)
        if (myRoleConfig->role == BoardRole::Leader && connMsg.role != BoardRole::Leader) {
            if (connMsg.isConnected) {
                // Handle connection
                if (leaderboard.find(connMsg.nodeId) == leaderboard.end()) {
                    leaderboard[connMsg.nodeId] = PlayerStats(connMsg.roleName);
                    Serial.printf("New player discovered via echo: %s\n", connMsg.roleName.c_str());
                }
                leaderboard[connMsg.nodeId].isConnected = true;
                leaderboard[connMsg.nodeId].playerName = connMsg.roleName;
                Serial.printf("Marked %s as connected via echo\n", connMsg.roleName.c_str());
            } else {
                // Handle disconnection
                if (leaderboard.find(connMsg.nodeId) != leaderboard.end()) {
                    leaderboard[connMsg.nodeId].isConnected = false;
                    Serial.printf("Marked %s as disconnected via echo\n", connMsg.roleName.c_str());
                }
            }
            
            updateLeaderDisplays();
        }
        
        // Update nodeRoleMap for all nodes (including Leaders for routing)
        if (connMsg.isConnected) {
            // Only add if not already present to prevent duplicates
            if (nodeRoleMap.find(connMsg.nodeId) == nodeRoleMap.end()) {
                nodeRoleMap[connMsg.nodeId] = connMsg.role;
                Serial.printf("Added to nodeRoleMap: %u -> %s\n", connMsg.nodeId, connMsg.roleName.c_str());
            } else {
                Serial.printf("NodeID %u already in nodeRoleMap, skipping\n", connMsg.nodeId);
            }
        } else {
            nodeRoleMap.erase(connMsg.nodeId);
            Serial.printf("Removed from nodeRoleMap: %u (%s)\n", connMsg.nodeId, connMsg.roleName.c_str());
        }
    }
    // Try to parse as PlayerMessage
    else if (PlayerMessage::isPlayerMessage(msg)) {
        PlayerMessage playerMsg = PlayerMessage::fromJson(msg);
        
        // Validate that the message's fromNodeId matches the actual sender
        if (playerMsg.fromNodeId != from) {
            Serial.printf("WARNING: Message fromNodeId (%u) doesn't match sender (%u). Using sender.\n", 
                         playerMsg.fromNodeId, from);
            // Continue processing with the correct 'from' value
        }
        
        Serial.printf("Parsed PlayerMessage: Score=%d, TurnPassed=%s, FromNode=%u (%s)\n", 
                     playerMsg.score, playerMsg.turnPassed ? "YES" : "NO", 
                     from, fromName.c_str());
        
        // IGNORE score submissions before game starts
        if (!gameStarted) {
            Serial.printf("Game not started yet - ignoring score submission from %s\n", fromName.c_str());
            return; // Exit early, don't process the score
        }
        
        // Update leaderboard - use 'from' (the actual sender) not playerMsg.fromNodeId
        if (leaderboard.find(from) == leaderboard.end()) {
            leaderboard[from] = PlayerStats(fromName);
        }
        
        // Store the offset score (points made in this turn)
        leaderboard[from].lastOffsetScore = playerMsg.score;
        // Add to total score
        leaderboard[from].totalScore += playerMsg.score;
        
        Serial.printf("Updated leaderboard for %s: Total=%d, Offset=%d\n", 
                     fromName.c_str(), leaderboard[from].totalScore, leaderboard[from].lastOffsetScore);
        
        // Display on leader board (only if leader)
        if (myRoleConfig->role == BoardRole::Leader) {
            updateLeaderDisplays();
            
            // Turn management: if player passed turn, broadcast next player
            if (playerMsg.turnPassed && gameStarted) {
                Serial.printf("Player %s passed their turn!\n", fromName.c_str());
                
                // Only process turn passing if it's actually their turn
                uint32_t currentTurnNodeId = getNodeIdForRole(currentTurn);
                Serial.printf("DEBUG: Current turn role: %d, NodeID: %u\n", (int)currentTurn, currentTurnNodeId);
                Serial.printf("DEBUG: Message sender NodeID: %u\n", from);
                
                if (from == currentTurnNodeId) {
                    Serial.printf("Turn pass confirmed for current player\n");
                    broadcastNextTurn();
                } else {
                    Serial.printf("Turn pass ignored - not current player's turn (current: %u, sender: %u)\n", 
                                 currentTurnNodeId, from);
                }
            } else if (!playerMsg.turnPassed && gameStarted) {
                // Player added score but didn't pass turn - remind all players of current turn
                Serial.printf("Player %s added score but didn't pass turn - refreshing displays\n", fromName.c_str());
                
                // Send current turn reminder to all players
                uint32_t currentTurnNodeId = getNodeIdForRole(currentTurn);
                String currentTurnName = getNodeName(currentTurnNodeId);
                TurnMessage turnMsg(currentTurnNodeId, currentTurnName);
                String jsonStr = turnMsg.toJson();
                mesh.sendBroadcast(jsonStr);
                Serial.printf("Sent turn reminder: %s\n", jsonStr.c_str());
            }
        }
    }
    // Enhanced TurnMessage handling with better validation
    // Try to parse as TurnMessage
    else if (TurnMessage::isTurnMessage(msg)) {
        TurnMessage turnMsg = TurnMessage::fromJson(msg);
        
        Serial.printf("Parsed TurnMessage: NextPlayer=%s (NodeID: %u)\n", 
                     turnMsg.nextPlayerName.c_str(), turnMsg.nextPlayerNodeId);
        Serial.printf("My NodeID: %u, My Role: %s\n", mesh.getNodeId(), myRoleConfig->name.c_str());
        
        // Validate nodeId is not 0
        if (turnMsg.nextPlayerNodeId == 0) {
            Serial.printf("ERROR: Turn message has invalid nodeId 0!\n");
            return;
        }
        
        // Check if it's my turn
        if (turnMsg.nextPlayerNodeId == mesh.getNodeId()) {
            Serial.println("IT'S MY TURN! Displaying BEEF");
            primaryDisplay.print("BEEF");
            Serial.printf("Set display to BEEF for %s\n", myRoleConfig->name.c_str());
        } else {
            Serial.printf("It's %s's turn (not mine), keeping current display\n", turnMsg.nextPlayerName.c_str());
            // When it's not my turn and game has started, show "----"
            if (gameStarted && myRoleConfig->role != BoardRole::Leader) {
                primaryDisplay.print("----");
                Serial.printf("Set display to ---- (not my turn)\n");
            }
        }
        
        // Update brightness for all displays
        updateDisplayBrightness();
    } 
    else {
        // Fallback for non-structured messages
        if (myRoleConfig->role != BoardRole::Leader) {
            primaryDisplay.print(msg);
        }
        Serial.println("Received unknown message format");
    }
}

// Enhanced updateDisplayBrightness with detailed debugging
void updateDisplayBrightness() {
    if (!gameStarted) {
        Serial.printf("Game not started, skipping brightness update\n");
        return;
    }
    
    if (myRoleConfig->role == BoardRole::Leader) {
        // Update leader board display brightness
        uint32_t currentTurnNodeId = getNodeIdForRole(currentTurn);
        Serial.printf("DEBUG: Updating leader brightness, current turn: %s (NodeID: %u)\n", 
                     getNodeName(currentTurnNodeId).c_str(), currentTurnNodeId);
        
        // Set all leader displays to 75% first
        Serial.printf("Setting all leader displays to 75%%\n");
        primaryDisplay.setBrightness(75);
        display2.setBrightness(75);
        display3.setBrightness(75);
        display4.setBrightness(75);
        
        // Set current player's display to 100%
        BoardRole currentPlayerRole = getNodeRole(currentTurnNodeId);
        Serial.printf("Current player role: %d\n", (int)currentPlayerRole);
        
        switch (currentPlayerRole) {
            case BoardRole::Player_Red:
                Serial.printf("Setting RED display to 100%%\n");
                primaryDisplay.setBrightness(100);
                break;
            case BoardRole::Player_Blue:
                Serial.printf("Setting BLUE display to 100%%\n");
                display2.setBrightness(100);
                break;
            case BoardRole::Player_Green:
                Serial.printf("Setting GREEN display to 100%%\n");
                display3.setBrightness(100);
                break;
            case BoardRole::Player_White:
                Serial.printf("Setting WHITE display to 100%%\n");
                display4.setBrightness(100);
                break;
            default:
                Serial.printf("Unknown current player role: %d\n", (int)currentPlayerRole);
                break;
        }
        
        Serial.printf("Updated leader display brightness, current player: %s\n", 
                     getNodeName(currentTurnNodeId).c_str());
    } else {
        // Update player board brightness
        uint32_t currentTurnNodeId = getNodeIdForRole(currentTurn);
        if (mesh.getNodeId() == currentTurnNodeId) {
            // It's my turn - 100% brightness
            primaryDisplay.setBrightness(100);
            Serial.printf("Set my display to 100%% brightness (my turn)\n");
        } else {
            // Not my turn - 75% brightness
            primaryDisplay.setBrightness(75);
            Serial.printf("Set my display to 75%% brightness (not my turn)\n");
        }
    }
}

// 4. Add a manual brightness test function for debugging
void testDisplayBrightness() {
    if (myRoleConfig->role == BoardRole::Leader) {
        Serial.printf("Testing brightness levels...\n");
        
        primaryDisplay.setBrightness(100);
        display2.setBrightness(75);
        display3.setBrightness(50);
        display4.setBrightness(25);
        
        Serial.printf("Set displays to: RED=100%%, BLUE=75%%, GREEN=50%%, WHITE=25%%\n");
    }
}

void setup() {
    Serial.begin(115200);


    // while (!Serial) {
    //     ;  // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN,
    //     //and
    //        // other 32u4 based boards.
    // }

    delay(5000);
        // TODO: do we need this Wire.begin?
    Wire.begin(5, 6);
    
    // Scan for I2C devices
    Serial.println("Scanning for I2C devices...");
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X (decimal: %d)\n", address, address);
        } else if (error == 4) {
            Serial.printf("Unknown error at address 0x%02X\n", address);
        }
    }
    Serial.println("I2C scan complete.");
    
    // Initialize score
    currentScore = 0;

    // Initialize WiFi first so we can read MAC address
    WiFi.mode(WIFI_MODE_APSTA);
    delay(100);

    uint8_t macAddress[6];
    readMacAddress(macAddress);
    myRole = myBoardRole(macAddress);
    myRoleConfig = getMyRoleConfig(myRole);
    Serial.printf("my role: %s\n", myRoleConfig->name.c_str());
    
    // Use the entire MAC address to create a deterministic nodeId that preserves role info
    // We'll use bytes 2-5 but store the full MAC for role lookup
    uint32_t customNodeId = (macAddress[2] << 24) | (macAddress[3] << 16) | (macAddress[4] << 8) | macAddress[5];
    Serial.printf("Generated custom nodeId from MAC: %u (from %02x:%02x:%02x:%02x)\n", 
                 customNodeId, macAddress[2], macAddress[3], macAddress[4], macAddress[5]);

    // Set callbacks BEFORE mesh.init() - re-enable connection callbacks
    Serial.println("Setting mesh callbacks...");
    mesh.onNewConnection(newConnectionCallback);
    mesh.onDroppedConnection(lostConnectionCallback);
    mesh.onReceive(receivedCallback);
    mesh.onChangedConnections(changedConnectionsCallback);
    mesh.onNodeTimeAdjusted(nodeTimeAdjustedCallback);
    
    // Enable debug messages
    // mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION | COMMUNICATION);
    
    Serial.println("Initializing mesh...");
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_MODE_APSTA, 1, customNodeId);
    
    // Set root AFTER init
    if (myRoleConfig.has_value() && myRoleConfig->role == BoardRole::Leader) {
        mesh.setRoot(true);
        Serial.println("Set as mesh root node (Leader)");
    } else {
        mesh.setRoot(false);
        Serial.println("Set as mesh child node (not Leader)");
    }
    
    peers.emplace(mesh.getNodeId());
    Serial.printf("My mesh node ID: %u\n", mesh.getNodeId());
    Serial.printf("Mesh subnet: %s\n", mesh.subConnectionJson().c_str());
    
    // Add ourselves to the role mapping
    nodeRoleMap[mesh.getNodeId()] = myRole;
    Serial.printf("My node info: ID=%u, Role=%s (%d)\n", 
                 mesh.getNodeId(), myRoleConfig->name.c_str(), (int)myRole);
    
    // DEBUG: Print initial nodeRoleMap
    Serial.println("=== Initial nodeRoleMap ===");
    for (auto& pair : nodeRoleMap) {
        Serial.printf("NodeID: %u -> Role: %d (%s)\n", 
                     pair.first, (int)pair.second, getNodeName(pair.first).c_str());
    }
    Serial.println("===========================");

    if (myRoleConfig.has_value()) {
        if (myRoleConfig->role == BoardRole::Leader) {
            primaryDisplay.setup(0x70);
            primaryDisplay.print("----");
            display2.setup(0x71);
            display2.print("----");
            display3.setup(0x72);
            display3.print("----");
            display4.setup(0x73);
            display4.print("----");
        } else {
            // All players start with "----" instead of their role names
            if (myRoleConfig->role == BoardRole::Player_Red) {
                primaryDisplay.setup(0x70);
            } else if (myRoleConfig->role == BoardRole::Player_Blue) {
                primaryDisplay.setup(0x70);
            } else if (myRoleConfig->role == BoardRole::Player_Green) {
                primaryDisplay.setup(0x70);
            } else if (myRoleConfig->role == BoardRole::Player_White) {
                primaryDisplay.setup(0x70);
            }
            
            primaryDisplay.print("----");
            Serial.printf("%s display initialized to ----\n", myRoleConfig->name.c_str());
        }
    }

    Serial.println("hardware setup started");

    buttonGrid.setup();
    encoder.setup();

    Serial.println("setup done");
}

void loop() {
    static unsigned long lastStatusPrint = 0;
    
    mesh.update();
    
    // Print status every 10 seconds
    if (millis() - lastStatusPrint > 10000) {
        Serial.print("Mesh status - Connected roles: ");
        
        // Debug: Print all entries in nodeRoleMap
        Serial.printf("\nDEBUG nodeRoleMap contents (%d entries):\n", nodeRoleMap.size());
        for (auto it = nodeRoleMap.begin(); it != nodeRoleMap.end(); ++it) {
            Serial.printf("  NodeID: %u -> Role: %d (%s)\n", 
                         it->first, (int)it->second, getNodeName(it->first).c_str());
        }
        
        for (auto it = nodeRoleMap.begin(); it != nodeRoleMap.end(); ++it) {
            if (it != nodeRoleMap.begin()) Serial.print(", ");
            String nodeName = getNodeName(it->first);
            Serial.printf("%s", nodeName.c_str());
        }
        Serial.printf(" | My ID: %u", mesh.getNodeId());
        
        if (myRoleConfig->role == BoardRole::Leader) {
            Serial.printf(" | Game: %s", gameStarted ? "STARTED" : "WAITING");
            int connectedCount = 0;
            for (auto& pair : leaderboard) {
                if (pair.second.isConnected) connectedCount++;
            }
            Serial.printf(" | Connected Players: %d", connectedCount);
        }
        Serial.println();
        
        lastStatusPrint = millis();
    }
    
    buttonGrid.loop();
    encoder.loop();
}
