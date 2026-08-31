#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Event.hpp>
#include <OtaUpdate.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <map>
#include <memory>

class Coordinator;
class BleDownlinkCallbacks;
class BleScanCallbacks;

// Battery-first BLE transport.  The fixed leaderboard is the central and every
// player is a peripheral; no Wi-Fi AP or mesh routing remains active.
class MyBle {
public:
    explicit MyBle(Coordinator* coordinator);
    ~MyBle();
    MyBle(const MyBle&) = delete;
    MyBle& operator=(const MyBle&) = delete;

    void setup();
    void loop();
    uint32_t getMyPeerId() const;
    bool hasLeader() const;
    void confirmLeader(uint16_t connectionHandle);
    void armOta();
    bool otaArmed() const;
    bool sendBroadcast(const String& message);
    bool sendTo(uint32_t nodeId, const String& message);

private:
    struct Peer {
        uint32_t nodeId;
        NimBLEClient* client;
        NimBLERemoteCharacteristic* downlink;
        bool announced;
    };

    Coordinator* coordinator;
    uint32_t peerId;
    NimBLEServer* server;
    NimBLECharacteristic* uplink;
    NimBLECharacteristic* downlink;
    std::unique_ptr<BleDownlinkCallbacks> downlinkCallbacks;
    std::unique_ptr<BleScanCallbacks> scanCallbacks;
    OtaUpdate ota;
    std::map<uint32_t, Peer> peers;
    std::array<uint32_t, 4> pendingLostPeers;
    size_t pendingLostPeerCount;
    SemaphoreHandle_t peersMutex;
    std::atomic<uint32_t> lastLeaderActivityMs;
    std::atomic<uint16_t> leaderConnectionHandle;
    std::atomic<bool> leaderConnected;
    std::atomic<bool> leaderLostPending;
    std::atomic<uint32_t> droppedMessages;
    uint32_t lastScanStartedMs;

    void receive(uint32_t from, const uint8_t* data, size_t length, uint16_t connectionHandle);
    void addPeer(NimBLEClient* client);
    void removePeer(NimBLEClient* client);
    void reconcilePeers();
    void flushLifecycleEvents();
    bool hasPeer(NimBLEClient* client) const;
    size_t peerCount() const;
    void beginScan();
    bool enqueue(const Event& event);
    bool emit(EventType type, uint32_t nodeId = 0);

    friend class BleDownlinkCallbacks;
    friend class BleServerCallbacks;
    friend class BleScanCallbacks;
};
