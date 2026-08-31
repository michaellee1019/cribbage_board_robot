#pragma once

// Message structure for player updates
#include <cstdint>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Protocol.hpp>
#include <utils.hpp>

struct PlayerMessage {
    int32_t score;
    bool turnPassed;
    uint32_t fromNodeId;
    uint32_t operationId;
    uint32_t gameId;
    
    // Constructor
    PlayerMessage(int32_t s = 0, bool t = false, uint32_t nodeId = 0,
                  uint32_t operation = 0, uint32_t game = 0)
        : score(s), turnPassed(t), fromNodeId(nodeId), operationId(operation), gameId(game) {
        DEBUG_PRINTF("DEBUG PlayerMessage created: score=%d, turnPassed=%s, fromNodeId=%u\n", 
                     s, t ? "true" : "false", nodeId);
    }
    
    // Serialize to JSON string
    String toJson() const {
        JsonDocument doc;
        doc["type"] = "player";
        doc["protocol"] = scorebot::kWireProtocolVersion;
        doc["score"] = score;
        doc["turnPassed"] = turnPassed;
        doc["fromNodeId"] = fromNodeId;
        doc["operationId"] = operationId;
        doc["game"] = gameId;
        
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
            doc["fromNodeId"].as<uint32_t>(),
            doc["operationId"].as<uint32_t>(),
            doc["game"].as<uint32_t>()
        );
    }
    
    // Validation updated for signed integers
    static bool isPlayerMessage(const String& jsonStr) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        return error == DeserializationError::Ok && 
               doc["type"].is<const char*>() && 
               doc["type"] == "player" &&
               doc["protocol"].is<uint16_t>() &&
               doc["protocol"].as<uint16_t>() == scorebot::kWireProtocolVersion &&
               doc["score"].is<int>() &&
               doc["turnPassed"].is<bool>() &&
               doc["fromNodeId"].is<unsigned int>() &&
               doc["operationId"].is<unsigned int>() &&
               doc["game"].is<unsigned int>();
    }
};

// Ephemeral, throttled visual wake-up. It never changes game state and is
// accepted only from a connected member of the current frozen roster.
struct PlayerActivityMessage {
    uint32_t fromNodeId;
    uint32_t gameId;

    PlayerActivityMessage(uint32_t nodeId = 0, uint32_t game = 0)
        : fromNodeId(nodeId), gameId(game) {}

    String toJson() const {
        JsonDocument doc;
        doc["type"] = "activity";
        doc["protocol"] = scorebot::kWireProtocolVersion;
        doc["fromNodeId"] = fromNodeId;
        doc["game"] = gameId;
        String output;
        serializeJson(doc, output);
        return output;
    }

    static PlayerActivityMessage fromJson(const String& jsonStr) {
        JsonDocument doc;
        deserializeJson(doc, jsonStr);
        return {doc["fromNodeId"].as<uint32_t>(), doc["game"].as<uint32_t>()};
    }

    static bool isPlayerActivityMessage(const String& jsonStr) {
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, jsonStr);
        return error == DeserializationError::Ok &&
               doc["type"].is<const char*>() && doc["type"] == "activity" &&
               doc["protocol"].is<uint16_t>() &&
               doc["protocol"].as<uint16_t>() == scorebot::kWireProtocolVersion &&
               doc["fromNodeId"].is<unsigned int>() &&
               doc["game"].is<unsigned int>();
    }
};
