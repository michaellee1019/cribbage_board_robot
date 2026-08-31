#include <MyBle.hpp>

#include <BoardRole.hpp>
#include <BoardIdentity.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>
#include <Protocol.hpp>
#include <utils.hpp>

#include <array>

constexpr char kServiceUuid[] = "c6a8619e-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kIdentityUuid[] = "c6a8619f-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kUplinkUuid[] = "c6a861a0-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kDownlinkUuid[] = "c6a861a1-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kProtocolUuid[] = "c6a861a2-2f9d-46bc-9a23-bb9c89a519be";
constexpr uint32_t kLeaderTimeoutMs = 15000;
constexpr uint32_t kScanDurationMs = 5000;
constexpr uint32_t kScanBackoffMs = 1000;
constexpr uint32_t kMaintenanceScanDurationMs = 2000;
constexpr uint32_t kMaintenanceScanBackoffMs = 5000;
constexpr uint32_t kConnectionSettleMs = 500;
constexpr uint32_t kRejectedPeerBackoffMs = 15000;
constexpr uint32_t kConnectionFailureBackoffMs = 2000;
constexpr uint32_t kConnectionWatchdogMs = 10000;
constexpr uint16_t kJoinMinInterval = 12;
constexpr uint16_t kJoinMaxInterval = 24;
constexpr uint16_t kJoinLatency = 0;
constexpr uint16_t kJoinTimeout = 400;
constexpr uint16_t kSteadyMinInterval = 80;
constexpr uint16_t kSteadyMaxInterval = 100;
constexpr uint16_t kSteadyLatency = 4;
constexpr uint16_t kSteadyTimeout = 600;
constexpr size_t kPlayerCount = 4;
constexpr size_t kTxQueueDepth = 8;
constexpr uint16_t kNoConnection = BLE_HS_CONN_HANDLE_NONE;

#if CONFIG_ESP32S3_UNIVERSAL_MAC_ADDRESSES == 2
constexpr uint8_t kBluetoothMacOffset = 1;
#else
constexpr uint8_t kBluetoothMacOffset = 2;
#endif

class ScopedSemaphore final {
public:
    explicit ScopedSemaphore(SemaphoreHandle_t semaphore) : semaphore(semaphore) {
        xSemaphoreTake(semaphore, portMAX_DELAY);
    }
    ~ScopedSemaphore() { xSemaphoreGive(semaphore); }
    ScopedSemaphore(const ScopedSemaphore&) = delete;
    ScopedSemaphore& operator=(const ScopedSemaphore&) = delete;

private:
    SemaphoreHandle_t semaphore;
};

bool isPlayerRole(BoardRole role) {
    return role == BoardRole::Player_Red || role == BoardRole::Player_Blue ||
           role == BoardRole::Player_Green || role == BoardRole::Player_White;
}

uint8_t roleMask(BoardRole role) {
    return isPlayerRole(role)
               ? static_cast<uint8_t>(1u << (static_cast<uint8_t>(role) -
                                             static_cast<uint8_t>(BoardRole::Player_Red)))
               : 0;
}

class BleDownlinkCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit BleDownlinkCallbacks(MyBle& owner) : owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
        const NimBLEAttValue value = characteristic->getValue();
        const NimBLEAddress address = connection.getAddress();
        const uint32_t sender = address.isPublic()
                                    ? getRoleConfigForBluetoothMac(
                                          static_cast<uint64_t>(address),
                                          kBluetoothMacOffset).nodeId
                                    : 0;
        owner.receive(sender, value.data(), value.length(), connection.getConnHandle());
    }

private:
    MyBle& owner;
};

class BleServerCallbacks final : public NimBLEServerCallbacks {
public:
    explicit BleServerCallbacks(MyBle& owner) : owner(owner) {}

    void onConnect(NimBLEServer*, NimBLEConnInfo& connection) override {
        DEBUG_PRINTF("BLE: inbound connection from %s handle=%u core=%d\n",
                     connection.getAddress().toString().c_str(),
                     static_cast<unsigned>(connection.getConnHandle()), xPortGetCoreID());
    }

    void onDisconnect(NimBLEServer*, NimBLEConnInfo& connection, int reason) override {
        DEBUG_PRINTF("BLE: inbound disconnect from %s handle=%u reason=%d\n",
                     connection.getAddress().toString().c_str(),
                     static_cast<unsigned>(connection.getConnHandle()), reason);
        owner.ota.onDisconnected(connection.getConnHandle());
        if (owner.coordinator->myRole() != BoardRole::Leader &&
            owner.leaderConnectionHandle.load() == connection.getConnHandle()) {
            owner.recordLinkLoss();
            owner.leaderConnectionHandle.store(kNoConnection);
            owner.leaderConnected.store(false);
            owner.leaderLostPending.store(true);
        }
    }

private:
    MyBle& owner;
};

class BleClientCallbacks final : public NimBLEClientCallbacks {
public:
    explicit BleClientCallbacks(MyBle& owner) : owner(owner) {}

    void onConnect(NimBLEClient* client) override {
        // Keep connectionPending asserted until reconcilePeers has completed
        // service discovery and subscription. Releasing it here can start a
        // second scan while NimBLE is still finalizing this connection.
        DEBUG_PRINTF("BLE: connected to %s core=%d\n",
                     client->getPeerAddress().toString().c_str(), xPortGetCoreID());
    }

    void onConnectFail(NimBLEClient* client, int reason) override {
        owner.connectionSettledMs.store(millis());
        owner.recordLinkLoss();
        owner.backOffPeer(client->getPeerAddress(), kConnectionFailureBackoffMs);
        owner.finishPendingConnection(client);
        DEBUG_PRINTF("BLE: connect failed to %s reason=%d\n",
                     client->getPeerAddress().toString().c_str(), reason);
    }

    void onDisconnect(NimBLEClient* client, int reason) override {
        owner.connectionSettledMs.store(millis());
        owner.recordLinkLoss(client);
        // A different established peer can disconnect while this client is
        // connecting. Only the client that owns the in-flight attempt may
        // release the scan/connect gate.
        owner.finishPendingConnection(client);
        DEBUG_PRINTF("BLE: disconnected from %s reason=%d\n",
                     client->getPeerAddress().toString().c_str(), reason);
    }

private:
    MyBle& owner;
};

class BleScanCallbacks final : public NimBLEScanCallbacks {
public:
    explicit BleScanCallbacks(MyBle& owner) : owner(owner) {}

    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (owner.coordinator->myRole() != BoardRole::Leader ||
            !device->isAdvertisingService(NimBLEUUID(kServiceUuid))) {
            return;
        }

        if (device->getAddress() == NimBLEDevice::getAddress()) {
            return;
        }
        NimBLEClient* client = NimBLEDevice::getClientByPeerAddress(device->getAddress());
        if (client != nullptr && client->isConnected()) {
            return;
        }
        if (owner.peerIsBackedOff(device->getAddress())) {
            return;
        }
        if (owner.connectionPending.exchange(true)) {
            return;
        }
        DEBUG_PRINTF("BLE: candidate %s rssi=%d dBm cached=%d\n",
                     device->getAddress().toString().c_str(), device->getRSSI(),
                     client != nullptr);
        // Do not initiate a GAP connection from NimBLE's scan callback. Copy
        // the address and let the application loop stop scanning and connect.
        // Publishing the request after the copy makes the handoff atomic.
        owner.pendingConnectionAddress = device->getAddress();
        owner.connectionAttemptStartedMs.store(millis());
        owner.connectionRequested.store(true);
    }

private:
    MyBle& owner;
};
MyBle::MyBle(Coordinator* coordinator)
    : coordinator(coordinator),
      peerId(0),
      server(nullptr),
      uplink(nullptr),
      downlink(nullptr),
      downlinkCallbacks(),
      scanCallbacks(),
      clientCallbacks(),
      ota(),
      peers(),
      rejectedPeers(),
      nextRejectedPeer(0),
      pendingLostPeers(),
      pendingLostPeerCount(0),
      intentionalDisconnects(),
      peersMutex(nullptr),
      txQueue(nullptr),
      lastLeaderActivityMs(0),
      leaderConnectionHandle(kNoConnection),
      leaderConnected(false),
      leaderLostPending(false),
      otaArmRequested(false),
      rosterFrozen(false),
      rosterMask(0),
      connectionPending(false),
      pendingConnectionClient(nullptr),
      pendingConnectionAddress(),
      connectionRequested(false),
      connectionAttemptStartedMs(0),
      connectionSettledMs(0),
      disconnectWindow(),
      powerIncreaseRequested(false),
      connectionPowerDbm(scorebot::ble_power::kInitialConnectionPowerDbm),
      lastScanStartedMs(0),
      lastAdvertisingCheckMs(0) {}

MyBle::~MyBle() {
    if (peersMutex != nullptr) {
        vSemaphoreDelete(peersMutex);
    }
    if (txQueue != nullptr) {
        vQueueDelete(txQueue);
    }
}

uint32_t MyBle::getMyPeerId() const {
    return peerId;
}

bool MyBle::hasLeader() const {
    return coordinator->myRole() == BoardRole::Leader || leaderConnected.load();
}

void MyBle::freezeRoster(uint8_t allowedRosterMask) {
    rosterMask.store(allowedRosterMask);
    rosterFrozen.store(true);
    // The BLE-owner loop performs any disconnects. GameState runs on a
    // separate task and must not issue NimBLE client operations concurrently.
}

void MyBle::openRoster() {
    rosterMask.store(0);
    rosterFrozen.store(false);
}

void MyBle::confirmLeader(uint16_t connectionHandle) {
    if (coordinator->myRole() == BoardRole::Leader || connectionHandle == kNoConnection) {
        return;
    }
    leaderConnectionHandle.store(connectionHandle);
    lastLeaderActivityMs.store(millis());
    if (!leaderConnected.exchange(true)) {
        DEBUG_PRINTF("BLE: authoritative leaderboard ready handle=%u\n",
                     static_cast<unsigned>(connectionHandle));
    }
}

void MyBle::armOta() {
    // The BLE-owner loop changes the OTA characteristic and advertising state.
    // This method is called from the UI dispatcher and only publishes intent.
    otaArmRequested.store(true);
}

bool MyBle::sleepAllowed() const {
    return !otaArmRequested.load() && !ota.isArmed() && !ota.isWriting();
}

void MyBle::shutdownForSleep() {
    // deinit(false) stops the host and controller without running destructors
    // against application-owned callback objects during the final sleep path.
    NimBLEDevice::deinit(false);
}

void MyBle::setup() {
    // Preserve the IDs used by the original painlessMesh role table. A raw
    // uint32_t cast selects the wrong end of ESP.getEfuseMac().
    peerId = scorebot::boardIdFromEfuseMac(ESP.getEfuseMac());
    const String deviceName = String("Scorebot-") + String(peerId, HEX);
    if (!NimBLEDevice::init(deviceName.c_str())) {
        FATAL_ERROR(ErrorCode::BLE_INIT_FAILED, "NimBLE initialization");
    }
    NimBLEDevice::setMTU(185);
    NimBLEDevice::setPower(connectionPowerDbm.load());
    // Use maximum power only for discovery/OTA advertising and connection
    // initiation. Once connected, the lower default power preserves battery.
    NimBLEDevice::setPower(9, NimBLETxPowerType::Advertise);
    NimBLEDevice::setPower(9, NimBLETxPowerType::Scan);
    peersMutex = xSemaphoreCreateMutex();
    CHECK_POINTER(peersMutex, ErrorCode::SEMAPHORE_CREATE_FAILED, "BLE peer mutex");
    txQueue = xQueueCreate(kTxQueueDepth, sizeof(TxRequest));
    CHECK_POINTER(txQueue, ErrorCode::QUEUE_CREATE_FAILED, "BLE transmission queue");

    server = NimBLEDevice::createServer();
    CHECK_POINTER(server, ErrorCode::MEMORY_ALLOCATION_FAILED, "BLE server");
    server->setCallbacks(new BleServerCallbacks(*this), true);
    server->advertiseOnDisconnect(true);
    NimBLEService* service = server->createService(kServiceUuid);
    CHECK_POINTER(service, ErrorCode::MEMORY_ALLOCATION_FAILED, "game BLE service");

    NimBLECharacteristic* identity = service->createCharacteristic(
        kIdentityUuid, NIMBLE_PROPERTY::READ, sizeof(peerId));
    CHECK_POINTER(identity, ErrorCode::MEMORY_ALLOCATION_FAILED, "BLE identity characteristic");
    identity->setValue(peerId);
    NimBLECharacteristic* protocol = service->createCharacteristic(
        kProtocolUuid, NIMBLE_PROPERTY::READ, sizeof(scorebot::kWireProtocolVersion));
    CHECK_POINTER(protocol, ErrorCode::MEMORY_ALLOCATION_FAILED, "BLE protocol characteristic");
    protocol->setValue(scorebot::kWireProtocolVersion);
    uplink = service->createCharacteristic(
        kUplinkUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        scorebot::kMaxWireMessageSize);
    downlink = service->createCharacteristic(
        kDownlinkUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        scorebot::kMaxWireMessageSize);
    CHECK_POINTER(uplink, ErrorCode::MEMORY_ALLOCATION_FAILED, "BLE uplink characteristic");
    CHECK_POINTER(downlink, ErrorCode::MEMORY_ALLOCATION_FAILED, "BLE downlink characteristic");
    downlinkCallbacks = std::make_unique<BleDownlinkCallbacks>(*this);
    downlink->setCallbacks(downlinkCallbacks.get());
    ota.setup(server);
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    // Flags + a 128-bit service UUID leave too little room for the complete
    // device name in the 31-byte advertisement. Put it in the scan response
    // so CoreBluetooth reports Scorebot-<id> instead of a nameless peripheral.
    advertising->enableScanResponse(true);
    advertising->addServiceUUID(kServiceUuid);
    advertising->setName(deviceName.c_str());
    // Advertise four times per second while waiting to join. Advertising ends
    // on connection, so this discovery reliability does not become a steady
    // battery cost during a game.
    advertising->setAdvertisingInterval(400);
    // Player boards must advertise so the leaderboard can discover them. The
    // battery-powered leaderboard only advertises when OTA is explicitly
    // armed; otherwise its radio remains receive-only while building a lobby.
    const bool advertisingStarted = coordinator->myRole() != BoardRole::Leader
                                        ? advertising->start()
                                        : false;
    DEBUG_PRINTF("BLE: advertising name=%s started=%d\n",
                 deviceName.c_str(), advertisingStarted);

    if (coordinator->myRole() == BoardRole::Leader) {
        clientCallbacks = std::make_unique<BleClientCallbacks>(*this);
        scanCallbacks = std::make_unique<BleScanCallbacks>(*this);
        beginScan();
    }
}

void MyBle::beginScan() {
    if (coordinator->myRole() != BoardRole::Leader || connectionPending.load()) {
        return;
    }
    NimBLEScan* scan = NimBLEDevice::getScan();
    const uint32_t now = millis();
    const uint32_t settledMs = connectionSettledMs.load();
    if (settledMs != 0 && now - settledMs < kConnectionSettleMs) {
        return;
    }
    const size_t peers = peerCount();
    const uint32_t scanBackoffMs = peers == 0 ? kScanBackoffMs : kMaintenanceScanBackoffMs;
    const uint32_t scanDurationMs = peers == 0 ? kScanDurationMs : kMaintenanceScanDurationMs;
    const size_t targetPeers = rosterFrozen.load()
                                   ? static_cast<size_t>(__builtin_popcount(rosterMask.load()))
                                   : kPlayerCount;
    if (scan->isScanning() || peers >= targetPeers ||
        (lastScanStartedMs != 0 && now - lastScanStartedMs < scanBackoffMs)) {
        return;
    }
    // A second player can be reported while an asynchronous connection to the
    // first is still pending. Keep duplicate reports enabled so that skipped
    // player gets another callback after the first connection completes.
    scan->setScanCallbacks(scanCallbacks.get(), true);
    // The service UUID is in the primary advertisement, so passive scanning
    // is sufficient and avoids needless scan-request transmissions.
    scan->setActiveScan(false);
    scan->setInterval(100);
    // A partial maintenance window can phase-lock with the players' one-second
    // advertisements and miss the same board forever, so keep the window
    // continuous during each bounded lobby scan.
    scan->setWindow(100);
    scan->setMaxResults(0);
    const bool started = scan->start(scanDurationMs, false, true);
    DEBUG_PRINTF("BLE: scan peers=%u duration=%lu started=%d\n",
                 static_cast<unsigned>(peers),
                 static_cast<unsigned long>(scanDurationMs),
                 started);
    lastScanStartedMs = now;
}

void MyBle::startPendingConnection() {
    if (!connectionRequested.exchange(false)) {
        return;
    }

    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan->isScanning()) {
        scan->stop();
    }

    NimBLEClient* client = NimBLEDevice::getClientByPeerAddress(pendingConnectionAddress);
    if (client != nullptr && client->isConnected()) {
        connectionPending.store(false);
        connectionAttemptStartedMs.store(0);
        return;
    }

    const bool newClient = client == nullptr;
    if (newClient) {
        client = NimBLEDevice::createClient(pendingConnectionAddress);
        if (client == nullptr) {
            connectionPending.store(false);
            connectionAttemptStartedMs.store(0);
            DEBUG_PRINTLN("BLE: could not allocate player client");
            return;
        }
        client->setClientCallbacks(clientCallbacks.get(), false);
        // Use a responsive interval only while establishing and discovering.
        // The connection returns to the battery-first parameters as soon as
        // the player is registered.
        client->setConnectionParams(
            kJoinMinInterval, kJoinMaxInterval, kJoinLatency, kJoinTimeout);
        client->setConnectTimeout(5000);
        NimBLEClient::Config config = client->getConfig();
        // A failed peer returns to the scan rotation after a short backoff.
        // Avoid spending three consecutive attempts on it while other players
        // are waiting to join.
        config.connectFailRetries = 0;
        client->setConfig(config);
    }

    pendingConnectionClient.store(client);
    connectionAttemptStartedMs.store(millis());
    // A previously verified client keeps its attribute cache across a normal
    // disconnect; a newly allocated client performs full discovery.
    if (!client->connect(newClient, true, true)) {
        finishPendingConnection(client);
        DEBUG_PRINTF("BLE: connect start failed for %s\n",
                     pendingConnectionAddress.toString().c_str());
        if (newClient) {
            NimBLEDevice::deleteClient(client);
        }
    }
}

void MyBle::recoverStalledConnection() {
    if (!connectionPending.load()) {
        return;
    }
    const uint32_t startedMs = connectionAttemptStartedMs.load();
    if (startedMs == 0 || millis() - startedMs < kConnectionWatchdogMs) {
        return;
    }
    NimBLEClient* client = pendingConnectionClient.load();
    if (client != nullptr && client->isConnected()) {
        // reconcilePeers will discover and register it on this loop.
        return;
    }

    DEBUG_PRINTF("BLE: recovering stalled connection to %s\n",
                 pendingConnectionAddress.toString().c_str());
    connectionRequested.store(false);
    pendingConnectionClient.store(nullptr);
    connectionPending.store(false);
    connectionAttemptStartedMs.store(0);
    connectionSettledMs.store(millis());
    backOffPeer(pendingConnectionAddress, kRejectedPeerBackoffMs);
    if (client != nullptr) {
        NimBLEDevice::deleteClient(client);
    }
}

void MyBle::addPeer(NimBLEClient* client) {
    const uint32_t discoveryStartedMs = millis();
    const bool hadCachedServices = !client->getServices().empty();
    NimBLERemoteService* service = client->getService(kServiceUuid);
    DEBUG_PRINTF("BLE: service discovered in %lu ms\n",
                 static_cast<unsigned long>(millis() - discoveryStartedMs));
    keepExistingPeersAlive();
    if (service == nullptr) {
        DEBUG_PRINTLN("BLE: player service discovery failed");
        NimBLEDevice::deleteClient(client);
        return;
    }
    const bool attributesCached =
        hadCachedServices && !service->getCharacteristics().empty();
    const size_t characteristicCount = attributesCached
                                           ? service->getCharacteristics().size()
                                           : service->getCharacteristics(true).size();
    DEBUG_PRINTF("BLE: %u characteristics ready at %lu ms cached=%d\n",
                 static_cast<unsigned>(characteristicCount),
                 static_cast<unsigned long>(millis() - discoveryStartedMs),
                 attributesCached);
    keepExistingPeersAlive();
    NimBLERemoteCharacteristic* identity = service->getCharacteristic(kIdentityUuid);
    DEBUG_PRINTF("BLE: identity discovered at %lu ms\n",
                 static_cast<unsigned long>(millis() - discoveryStartedMs));
    keepExistingPeersAlive();
    NimBLERemoteCharacteristic* protocol = service->getCharacteristic(kProtocolUuid);
    DEBUG_PRINTF("BLE: protocol discovered at %lu ms\n",
                 static_cast<unsigned long>(millis() - discoveryStartedMs));
    keepExistingPeersAlive();
    NimBLERemoteCharacteristic* remoteUplink = service->getCharacteristic(kUplinkUuid);
    DEBUG_PRINTF("BLE: uplink discovered at %lu ms\n",
                 static_cast<unsigned long>(millis() - discoveryStartedMs));
    keepExistingPeersAlive();
    NimBLERemoteCharacteristic* remoteDownlink = service->getCharacteristic(kDownlinkUuid);
    DEBUG_PRINTF("BLE: downlink discovered at %lu ms\n",
                 static_cast<unsigned long>(millis() - discoveryStartedMs));
    keepExistingPeersAlive();
    if (identity == nullptr || protocol == nullptr || remoteUplink == nullptr || remoteDownlink == nullptr) {
        DEBUG_PRINTLN("BLE: player service is missing required characteristics");
        if (client->isConnected() && !attributesCached) {
            backOffPeer(client->getPeerAddress(), kRejectedPeerBackoffMs);
        }
        NimBLEDevice::deleteClient(client);
        return;
    }

    const uint32_t nodeId = identity->readValue<uint32_t>();
    const NimBLEAddress transportAddress = client->getPeerAddress();
    const uint32_t transportNodeId = transportAddress.isPublic()
                                         ? getRoleConfigForBluetoothMac(
                                               static_cast<uint64_t>(transportAddress),
                                               kBluetoothMacOffset).nodeId
                                         : 0;
    const uint16_t remoteProtocol = protocol->readValue<uint16_t>();
    const BoardRole role = getRoleConfig(nodeId).role;
    const uint8_t playerMask = roleMask(role);
    const bool rosterRejected =
        rosterFrozen.load() && (rosterMask.load() & playerMask) == 0;
    if (remoteProtocol != scorebot::kWireProtocolVersion || nodeId == 0 ||
        nodeId != transportNodeId ||
        nodeId == peerId || !isPlayerRole(role) ||
        rosterRejected) {
        DEBUG_PRINTF(
            "BLE: rejected player identity=%08lx protocol=%u expected=%u frozen=%d roster=0x%02x\n",
            static_cast<unsigned long>(nodeId), static_cast<unsigned>(remoteProtocol),
            static_cast<unsigned>(scorebot::kWireProtocolVersion), rosterFrozen.load(),
            static_cast<unsigned>(rosterMask.load()));
        // A cached handle can become stale after player OTA. Delete it and
        // allow the next scan to retry with a fresh database immediately.
        if (!attributesCached || rosterRejected) {
            backOffPeer(client->getPeerAddress(), kRejectedPeerBackoffMs);
        }
        NimBLEDevice::deleteClient(client);
        return;
    }

    bool rejectedAfterFreeze = false;
    bool duplicate = false;
    {
        ScopedSemaphore lock(peersMutex);
        // Re-check under the peer lock: Start may freeze the roster after
        // service discovery's earlier check but before this insertion.
        rejectedAfterFreeze = rosterFrozen.load() &&
                              (rosterMask.load() & playerMask) == 0;
        duplicate = peers.count(nodeId) != 0;
        if (!rejectedAfterFreeze && !duplicate) {
            peers.emplace(nodeId, Peer{nodeId, client, remoteDownlink, false});
        }
    }
    if (rejectedAfterFreeze || duplicate) {
        DEBUG_PRINTF("BLE: %s player identity %08lx after discovery\n",
                     rejectedAfterFreeze ? "roster-rejected" : "duplicate",
                     static_cast<unsigned long>(nodeId));
        if (rejectedAfterFreeze) {
            backOffPeer(client->getPeerAddress(), kRejectedPeerBackoffMs);
        }
        NimBLEDevice::deleteClient(client);
        return;
    }
    if (!remoteUplink->subscribe(true,
        [this, nodeId](NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
            receive(nodeId, data, length, kNoConnection);
        })) {
        {
            ScopedSemaphore lock(peersMutex);
            peers.erase(nodeId);
        }
        NimBLEDevice::deleteClient(client);
        return;
    }
    const uint32_t joinMs = millis() - connectionAttemptStartedMs.load();
    const int rssi = client->getRssi();
    const bool steadyParamsRequested = client->updateConnParams(
        kSteadyMinInterval, kSteadyMaxInterval, kSteadyLatency,
        kSteadyTimeout);
    connectionSettledMs.store(millis());
    finishPendingConnection(client);
    DEBUG_PRINTF(
        "BLE: registered player %08lx join=%lu ms rssi=%d dBm steady=%d\n",
        static_cast<unsigned long>(nodeId), static_cast<unsigned long>(joinMs),
        rssi, steadyParamsRequested);
}

void MyBle::finishPendingConnection(NimBLEClient* client) {
    NimBLEClient* expected = client;
    if (client != nullptr && pendingConnectionClient.compare_exchange_strong(expected, nullptr)) {
        connectionPending.store(false);
        connectionAttemptStartedMs.store(0);
    }
}

void MyBle::removePeer(NimBLEClient* client) {
    connectionSettledMs.store(millis());
    uint32_t nodeId = 0;
    {
        ScopedSemaphore lock(peersMutex);
        for (auto it = peers.begin(); it != peers.end(); ++it) {
            if (it->second.client == client) {
                nodeId = it->first;
                peers.erase(it);
                break;
            }
        }
    }
    if (nodeId != 0) {
        DEBUG_PRINTF("BLE: removed player %08lx\n", static_cast<unsigned long>(nodeId));
        ScopedSemaphore lock(peersMutex);
        bool alreadyPending = false;
        for (size_t index = 0; index < pendingLostPeerCount; ++index) {
            alreadyPending = alreadyPending || pendingLostPeers[index] == nodeId;
        }
        if (!alreadyPending && pendingLostPeerCount < pendingLostPeers.size()) {
            pendingLostPeers[pendingLostPeerCount++] = nodeId;
        }
    }
    // Preserve the verified player's attribute cache for a fast reconnect.
    // addPeer validates live identity and protocol values; a stale cache after
    // OTA is discarded and retried fresh.
}

bool MyBle::hasPeer(NimBLEClient* client) const {
    ScopedSemaphore lock(peersMutex);
    for (const auto& [nodeId, peer] : peers) {
        if (peer.client == client) {
            return true;
        }
    }
    return false;
}

bool MyBle::peerIsBackedOff(const NimBLEAddress& address) const {
    const uint32_t now = millis();
    ScopedSemaphore lock(peersMutex);
    for (const RejectedPeer& rejected : rejectedPeers) {
        if (rejected.address == address &&
            static_cast<int32_t>(rejected.retryAfterMs - now) > 0) {
            return true;
        }
    }
    return false;
}

void MyBle::backOffPeer(const NimBLEAddress& address, uint32_t durationMs) {
    ScopedSemaphore lock(peersMutex);
    rejectedPeers[nextRejectedPeer] = {address, millis() + durationMs};
    nextRejectedPeer = (nextRejectedPeer + 1) % rejectedPeers.size();
}

void MyBle::recordLinkLoss(NimBLEClient* client) {
    if (client != nullptr) {
        ScopedSemaphore lock(peersMutex);
        for (NimBLEClient*& intentional : intentionalDisconnects) {
            if (intentional == client) {
                intentional = nullptr;
                return;
            }
        }
    }

    // Server and client disconnect callbacks run on NimBLE's host task, so
    // this window has a single writer. The application loop consumes only the
    // atomic request raised when the threshold is reached.
    const auto result = scorebot::ble_power::recordUnexpectedDisconnect(
        disconnectWindow, millis());
    disconnectWindow = result.window;
    if (result.increasePower &&
        connectionPowerDbm.load() < scorebot::ble_power::kMaximumConnectionPowerDbm) {
        powerIncreaseRequested.store(true);
    }
}

void MyBle::keepExistingPeersAlive() {
    if (coordinator->myRole() == BoardRole::Leader) {
        // Service discovery can take long enough to starve replication. Route
        // this maintenance heartbeat through the same state lock as the UI
        // dispatcher rather than racing GameState from the BLE loop.
        coordinator->serviceStateHeartbeat();
    }
}

size_t MyBle::peerCount() const {
    ScopedSemaphore lock(peersMutex);
    if (!rosterFrozen.load()) {
        return peers.size();
    }
    size_t count = 0;
    const uint8_t allowed = rosterMask.load();
    for (const auto& [nodeId, peer] : peers) {
        (void)peer;
        if ((allowed & roleMask(getRoleConfig(nodeId).role)) != 0) {
            ++count;
        }
    }
    return count;
}

void MyBle::disconnectExcludedPeers() {
    if (!rosterFrozen.load()) {
        return;
    }
    std::array<NimBLEClient*, kPlayerCount> excluded{};
    size_t count = 0;
    const uint8_t allowed = rosterMask.load();
    {
        ScopedSemaphore lock(peersMutex);
        for (const auto& [nodeId, peer] : peers) {
            if ((allowed & roleMask(getRoleConfig(nodeId).role)) == 0 &&
                count < excluded.size()) {
                excluded[count++] = peer.client;
            }
        }
        for (size_t index = 0; index < count; ++index) {
            bool marked = false;
            for (NimBLEClient* intentional : intentionalDisconnects) {
                marked = marked || intentional == excluded[index];
            }
            if (marked) {
                continue;
            }
            for (NimBLEClient*& intentional : intentionalDisconnects) {
                if (intentional == nullptr) {
                    intentional = excluded[index];
                    break;
                }
            }
        }
    }
    for (size_t index = 0; index < count; ++index) {
        if (excluded[index]->isConnected()) {
            excluded[index]->disconnect();
        }
    }
}

void MyBle::reconcilePeers() {
    for (NimBLEClient* client : NimBLEDevice::getConnectedClients()) {
        if (!hasPeer(client)) {
            addPeer(client);
        }
    }

    std::array<NimBLEClient*, kPlayerCount> disconnected{};
    size_t count = 0;
    {
        ScopedSemaphore lock(peersMutex);
        for (const auto& [nodeId, peer] : peers) {
            if (!peer.client->isConnected() && count < disconnected.size()) {
                disconnected[count++] = peer.client;
            }
        }
    }
    for (size_t index = 0; index < count; ++index) {
        removePeer(disconnected[index]);
    }
    flushLifecycleEvents();
}

void MyBle::flushLifecycleEvents() {
    std::array<uint32_t, kPlayerCount> newPeers{};
    std::array<uint32_t, kPlayerCount> lostPeers{};
    size_t newCount = 0;
    size_t lostCount = 0;
    {
        ScopedSemaphore lock(peersMutex);
        for (const auto& [nodeId, peer] : peers) {
            if (!peer.announced && newCount < newPeers.size()) {
                newPeers[newCount++] = nodeId;
            }
        }
        for (size_t index = 0; index < pendingLostPeerCount; ++index) {
            lostPeers[lostCount++] = pendingLostPeers[index];
        }
    }

    // A queued disconnect must precede a reconnect for the same identity.
    // Otherwise NewPeer is ignored as already connected and the following
    // LostPeer leaves the live transport permanently absent from GameState.
    for (size_t index = 0; index < lostCount; ++index) {
        if (!emit(EventType::LostPeer, lostPeers[index])) {
            continue;
        }
        ScopedSemaphore lock(peersMutex);
        for (size_t pending = 0; pending < pendingLostPeerCount; ++pending) {
            if (pendingLostPeers[pending] != lostPeers[index]) {
                continue;
            }
            for (size_t next = pending + 1; next < pendingLostPeerCount; ++next) {
                pendingLostPeers[next - 1] = pendingLostPeers[next];
            }
            --pendingLostPeerCount;
            break;
        }
    }
    for (size_t index = 0; index < newCount; ++index) {
        bool lostStillPending = false;
        {
            ScopedSemaphore lock(peersMutex);
            for (size_t pending = 0; pending < pendingLostPeerCount; ++pending) {
                lostStillPending = lostStillPending ||
                                   pendingLostPeers[pending] == newPeers[index];
            }
        }
        if (lostStillPending) {
            // If the event queue filled while emitting LostPeer, do not let a
            // newly freed slot accept NewPeer first. Retry both next loop.
            continue;
        }
        if (emit(EventType::NewPeer, newPeers[index])) {
            ScopedSemaphore lock(peersMutex);
            const auto peer = peers.find(newPeers[index]);
            if (peer != peers.end()) {
                peer->second.announced = true;
            }
        }
    }
}

bool MyBle::enqueueTransmission(uint32_t nodeId, const String& message, bool broadcast) {
    if (message.length() > scorebot::kMaxWireMessageSize || txQueue == nullptr) {
        return false;
    }
    TxRequest request{};
    request.nodeId = nodeId;
    request.length = static_cast<uint16_t>(message.length());
    request.broadcast = broadcast;
    memcpy(request.message, message.c_str(), request.length);
    return xQueueSend(txQueue, &request, 0) == pdPASS;
}

bool MyBle::sendTo(uint32_t nodeId, const String& message) {
    return enqueueTransmission(nodeId, message, false);
}

bool MyBle::sendBroadcast(const String& message) {
    return enqueueTransmission(0, message, true);
}

bool MyBle::transmitTo(uint32_t nodeId, const char* message, size_t length) {
    if (coordinator->myRole() != BoardRole::Leader) {
        const uint16_t connection = leaderConnectionHandle.load();
        if (!leaderConnected.load() || connection == kNoConnection || uplink == nullptr) {
            return false;
        }
        uplink->setValue(reinterpret_cast<const uint8_t*>(message), length);
        return uplink->notify(connection);
    }

    if (rosterFrozen.load() &&
        (rosterMask.load() & roleMask(getRoleConfig(nodeId).role)) == 0) {
        return false;
    }

    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* remoteDownlink = nullptr;
    {
        ScopedSemaphore lock(peersMutex);
        const auto peer = peers.find(nodeId);
        if (peer != peers.end()) {
            client = peer->second.client;
            remoteDownlink = peer->second.downlink;
        }
    }
    // BLE writes can synchronously wait on the NimBLE host task (especially
    // above one ATT packet). Never hold peersMutex across that wait because
    // host callbacks also use it.
    return client != nullptr && client->isConnected() && remoteDownlink != nullptr &&
           remoteDownlink->writeValue(
               reinterpret_cast<const uint8_t*>(message), length, false);
}

void MyBle::drainTransmissions() {
    TxRequest request{};
    while (xQueueReceive(txQueue, &request, 0) == pdPASS) {
        if (!request.broadcast || coordinator->myRole() != BoardRole::Leader) {
            const uint32_t target = request.broadcast
                                        ? getNodeIdForRole(BoardRole::Leader)
                                        : request.nodeId;
            (void)transmitTo(target, request.message, request.length);
            continue;
        }

        std::array<uint32_t, kPlayerCount> nodeIds{};
        size_t count = 0;
        const bool frozen = rosterFrozen.load();
        const uint8_t allowed = rosterMask.load();
        {
            ScopedSemaphore lock(peersMutex);
            for (const auto& [nodeId, peer] : peers) {
                (void)peer;
                if (frozen && (allowed & roleMask(getRoleConfig(nodeId).role)) == 0) {
                    continue;
                }
                if (count < nodeIds.size()) {
                    nodeIds[count++] = nodeId;
                }
            }
        }
        for (size_t index = 0; index < count; ++index) {
            (void)transmitTo(nodeIds[index], request.message, request.length);
        }
    }
}

void MyBle::receive(uint32_t from, const uint8_t* data, size_t length, uint16_t connectionHandle) {
    if (length == 0 || length >= sizeof(MessageReceivedEvent::message)) {
        return;
    }
    Event event{};
    event.type = EventType::MessageReceived;
    event.messageReceived.peerId = from;
    event.messageReceived.connectionHandle = connectionHandle;
    memcpy(event.messageReceived.message, data, length);
    event.messageReceived.message[length] = '\0';
    (void)enqueue(event);
}

bool MyBle::enqueue(const Event& event) {
    return coordinator->enqueueEvent(event);
}

bool MyBle::emit(EventType type, uint32_t nodeId) {
    Event event{};
    event.type = type;
    if (type == EventType::NewPeer) {
        event.newPeer.peerId = nodeId;
    } else if (type == EventType::LostPeer) {
        event.lostPeer.peerId = nodeId;
    }
    return enqueue(event);
}

void MyBle::loop() {
    if (otaArmRequested.exchange(false)) {
        ota.arm();
    }
    ota.loop();
    if (coordinator->myRole() == BoardRole::Leader) {
        disconnectExcludedPeers();
    }
    drainTransmissions();
    const uint32_t now = millis();
    if (powerIncreaseRequested.exchange(false)) {
        const int8_t nextPower =
            scorebot::ble_power::nextConnectionPowerDbm(
                connectionPowerDbm.load());
        if (NimBLEDevice::setPower(nextPower, NimBLETxPowerType::Connection)) {
            connectionPowerDbm.store(nextPower);
            DEBUG_PRINTF(
                "BLE: raised connection TX power to %d dBm after repeated disconnects\n",
                connectionPowerDbm.load());
        }
    }
    if (server != nullptr && now - lastAdvertisingCheckMs >= 1000) {
        lastAdvertisingCheckMs = now;
        NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
        const bool unpairedPlayer = coordinator->myRole() != BoardRole::Leader &&
                                    server->getConnectedCount() == 0;
        const bool shouldAdvertise = unpairedPlayer || ota.isArmed();
        if (shouldAdvertise && !advertising->isAdvertising()) {
            advertising->start();
        } else if (!shouldAdvertise && advertising->isAdvertising()) {
            advertising->stop();
        }
    }
    if (coordinator->myRole() == BoardRole::Leader) {
        reconcilePeers();
        recoverStalledConnection();
        startPendingConnection();
        beginScan();
        return;
    }

    if (leaderConnected.load() && now - lastLeaderActivityMs.load() > kLeaderTimeoutMs) {
        DEBUG_PRINTF("BLE: leaderboard silent for %lu ms\n",
                     static_cast<unsigned long>(now - lastLeaderActivityMs.load()));
        leaderConnected.store(false);
        leaderConnectionHandle.store(kNoConnection);
        leaderLostPending.store(true);
    }
    if (leaderLostPending.load() && emit(EventType::BleLeaderLost)) {
        leaderLostPending.store(false);
    }
}
