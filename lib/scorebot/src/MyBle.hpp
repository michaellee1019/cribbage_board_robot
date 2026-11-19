#pragma once

#include <Arduino.h>
#include <BlePowerRules.hpp>
#include <NimBLEDevice.h>
#include <Event.hpp>
#include <LeaderboardBroadcast.hpp>
#include <OtaUpdate.hpp>
#include <Protocol.hpp>
#include <PairingRules.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <map>
#include <memory>

class Coordinator;
class BleDownlinkCallbacks;
class BleScanCallbacks;
class BleClientCallbacks;

// The elected authority is central. Players and mirrored leaderboards are
// peripherals; roles and membership are established through runtime discovery.
class MyBle {
public:
    enum class TransmissionCompletion : uint8_t {
        Pending,
        Succeeded,
        Failed,
    };

    explicit MyBle(Coordinator* coordinator);
    ~MyBle();
    MyBle(const MyBle&) = delete;
    MyBle& operator=(const MyBle&) = delete;

    void setup(uint8_t displayCount);
    BoardRole localRole() const { return configuredRole.load(); }
    bool isAuthority() const { return localIsAuthority.load(); }
    uint32_t authorityNodeId() const { return selectedAuthority.load(); }
    uint32_t sessionId() const;
    uint8_t pairingId() const;
    BoardRole peerRole(uint32_t node) const;
    bool isControllerPeer(uint32_t node) const;
    bool acceptsLeader(uint32_t node) const;
    bool pairingUiActive() const { return pairingPrompt.load(); }
    void rotatePairing(int32_t delta);
    void confirmPairing();
    void chooseColor();
    void clearPairing();
    void renderPairingUi(uint32_t now);
    void loop();
    void publishLeaderboard(const scorebot::LeaderboardBroadcast& leaderboard);
    uint32_t getMyPeerId() const;
    bool hasLeader() const;
    void confirmLeader(uint16_t connectionHandle);
    void freezeRoster(uint8_t rosterMask);
    void openRoster();
    void armOta();
    bool otaActive() const;
    bool otaWriting() const;
    bool sleepAllowed() const;
    bool transmissionsPending() const;
    void beginSleepQuiesce();
    void cancelSleepQuiesce();
    void beginRestartQuiesce();
    void shutdownForSleep();
    bool sendBroadcast(const String& message);
    uint32_t sendBroadcastTracked(const String& message, uint32_t retryMs);
    bool sendTo(uint32_t nodeId, const String& message);
    TransmissionCompletion transmissionCompletion(uint32_t generation) const;
    uint32_t transmissionCompletedAtMs(uint32_t generation) const;

private:
    struct RejectedPeer {
        NimBLEAddress address;
        uint32_t retryAfterMs;
    };

    struct Peer {
        uint32_t nodeId;
        NimBLEClient* client;
        NimBLERemoteCharacteristic* downlink;
        bool announced;
        bool display;
        bool controller;
        BoardRole role;
    };

    struct TxRequest {
        uint32_t nodeId;
        uint32_t generation;
        uint32_t retryUntilMs;
        uint32_t nextAttemptMs;
        uint32_t lastSuccessfulAtMs;
        uint16_t length;
        bool broadcast;
        bool tracked;
        bool anySucceeded;
        char message[scorebot::kMaxWireMessageSize];
    };

    Coordinator* coordinator;
    uint32_t peerId;
    NimBLEServer* server;
    NimBLECharacteristic* uplink;
    NimBLECharacteristic* downlink;
    NimBLECharacteristic* pairingCharacteristic{nullptr};
    struct SeenBoard {
        scorebot::Discovery info{};
        NimBLEAddress address{};
        uint32_t seenAtMs{0};
    };
    scorebot::Discovery localDiscovery{};
    scorebot::PeerDirectory directory{};
    std::array<SeenBoard, 16> seenBoards{};
    QueueHandle_t discoveryQueue{nullptr};
    std::atomic<BoardRole> configuredRole{BoardRole::Unknown};
    std::atomic<bool> localIsAuthority{false};
    std::atomic<uint32_t> selectedAuthority{0};
    std::atomic<bool> pairingPrompt{true};
    std::atomic<int32_t> pairingRotation{0};
    std::atomic<bool> pairingConfirmed{false};
    std::atomic<bool> colorRequested{false};
    NimBLEAddress selectedLeaderAddress{};
    bool leaderAddressKnown{false};
    bool choosingColor{false};
    bool startupDiscovery{true};
    bool savedSelection{false};
    BoardRole suggestedColor{BoardRole::Player_Red};
    size_t selectedSessionIndex{0};
    uint32_t lastPairingFrame{0xffffffff};
    scorebot::DiscoveryPacket advertisedDiscovery{};
    scorebot::Discovery discovery() const;
    void observe(const NimBLEAdvertisedDevice* device);
    void processDiscoveries();
    void finishStartupDiscovery();
    void servicePairing();
    void savePairing();
    void initializePairing();
    void updatePairingAdvertisement();
    uint32_t senderForConnection(const NimBLEConnInfo& connection) const;
    std::unique_ptr<BleDownlinkCallbacks> downlinkCallbacks;
    std::unique_ptr<BleScanCallbacks> scanCallbacks;
    std::unique_ptr<BleClientCallbacks> clientCallbacks;
    OtaUpdate ota;
    std::map<uint32_t, Peer> peers;
    std::array<RejectedPeer, 4> rejectedPeers;
    size_t nextRejectedPeer;
    std::array<uint32_t, 4> pendingLostPeers;
    size_t pendingLostPeerCount;
    std::array<NimBLEClient*, 4> intentionalDisconnects;
    SemaphoreHandle_t peersMutex;
    SemaphoreHandle_t eventAdmissionMutex;
    QueueHandle_t txQueue;
    std::atomic<uint32_t> outstandingTransmissions;
    std::atomic<uint32_t> nextTransmissionGeneration;
    std::atomic<uint32_t> trackedCompletedGeneration;
    std::atomic<uint32_t> trackedSuccessfulGeneration;
    std::atomic<uint32_t> trackedCompletedAtMs;
    std::atomic<bool> transportQuiescing;
    std::atomic<uint32_t> lastLeaderActivityMs;
    std::atomic<uint16_t> leaderConnectionHandle;
    std::atomic<bool> leaderConnected;
    std::atomic<bool> leaderLostPending;
    std::atomic<bool> otaArmRequested;
    bool otaTransportActive;
    std::atomic<bool> rosterFrozen;
    std::atomic<uint8_t> rosterMask;
    std::atomic<bool> connectionPending;
    std::atomic<NimBLEClient*> pendingConnectionClient;
    NimBLEAddress pendingConnectionAddress;
    std::atomic<bool> connectionRequested;
    std::atomic<uint32_t> connectionAttemptStartedMs;
    std::atomic<uint32_t> connectionSettledMs;
    scorebot::ble_power::DisconnectWindow disconnectWindow;
    std::atomic<bool> powerIncreaseRequested;
    std::atomic<int8_t> connectionPowerDbm;
    uint32_t lastScanStartedMs;
    uint32_t lastAdvertisingCheckMs;
    uint32_t lastLeaderboardPublishMs{0};
    scorebot::LeaderboardPacket advertisedLeaderboard{};

    void receive(uint32_t from, const uint8_t* data, size_t length, uint16_t connectionHandle);
    void addPeer(NimBLEClient* client);
    void removePeer(NimBLEClient* client);
    void reconcilePeers();
    void disconnectExcludedPeers();
    void flushLifecycleEvents();
    void drainTransmissions();
    bool transmitTo(uint32_t nodeId, const char* message, size_t length);
    bool enqueueTransmission(uint32_t nodeId, const String& message, bool broadcast);
    uint32_t enqueueTrackedTransmission(
        uint32_t nodeId, const String& message, bool broadcast,
        uint32_t retryMs);
    uint32_t enqueueTransmissionRequest(
        uint32_t nodeId, const String& message, bool broadcast,
        bool tracked, uint32_t retryMs);
    void beginTransportQuiesce();
    bool hasPeer(NimBLEClient* client) const;
    bool peerIsBackedOff(const NimBLEAddress& address) const;
    void backOffPeer(const NimBLEAddress& address, uint32_t durationMs);
    void recordLinkLoss(NimBLEClient* client = nullptr);
    void keepExistingPeersAlive();
    void enterOtaTransportMode();
    void maintainOtaTransportMode();
    void exitOtaTransportMode();
    void configureAdvertising(bool otaOnly);
    void updateAdvertising(bool otaOnly);
    size_t peerCount() const;
    void beginScan();
    void startPendingConnection();
    void recoverStalledConnection();
    void finishPendingConnection(NimBLEClient* client);
    bool enqueue(const Event& event);
    bool emit(EventType type, uint32_t nodeId = 0);

    friend class BleDownlinkCallbacks;
    friend class BleServerCallbacks;
    friend class BleScanCallbacks;
    friend class BleClientCallbacks;
};
