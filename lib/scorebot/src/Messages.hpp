// Message structure for player updates
#include <cstdint>
#include <Arduino.h>
#include <ArduinoJson.h>

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
            doc["score"].as<int32_t>(),
            doc["turnPassed"].as<bool>(),
            doc["fromNodeId"].as<uint32_t>()
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

