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
constexpr uint32_t kConnectionWatchdogMs = 10000;
constexpr size_t kPlayerCount = 4;
constexpr uint16_t kNoConnection = BLE_HS_CONN_HANDLE_NONE;

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

class BleDownlinkCallbacks final : public NimBLECharacteristicCallbacks {
public:
    explicit BleDownlinkCallbacks(MyBle& owner) : owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
        const NimBLEAttValue value = characteristic->getValue();
        owner.receive(getNodeIdForRole(BoardRole::Leader), value.data(), value.length(),
                      connection.getConnHandle());
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
        owner.finishPendingConnection(client);
        DEBUG_PRINTF("BLE: connect failed to %s reason=%d\n",
                     client->getPeerAddress().toString().c_str(), reason);
    }

    void onDisconnect(NimBLEClient* client, int reason) override {
        owner.connectionSettledMs.store(millis());
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
      peersMutex(nullptr),
      lastLeaderActivityMs(0),
      leaderConnectionHandle(kNoConnection),
      leaderConnected(false),
      leaderLostPending(false),
      droppedMessages(0),
      rosterFrozen(false),
      rosterMask(0),
      connectionPending(false),
      pendingConnectionClient(nullptr),
      pendingConnectionAddress(),
      connectionRequested(false),
      connectionAttemptStartedMs(0),
      connectionSettledMs(0),
      lastScanStartedMs(0),
      lastAdvertisingCheckMs(0) {}

MyBle::~MyBle() {
    if (peersMutex != nullptr) {
        vSemaphoreDelete(peersMutex);
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
    if (coordinator->myRole() == BoardRole::Leader) {
        NimBLEScan* scan = NimBLEDevice::getScan();
        if (peerCount() >= static_cast<size_t>(__builtin_popcount(allowedRosterMask)) &&
            scan->isScanning()) {
            scan->stop();
        }
    }
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
    ota.arm();
    if (server != nullptr && coordinator->myRole() == BoardRole::Leader && ota.isArmed()) {
        NimBLEDevice::getAdvertising()->start();
    }
}

bool MyBle::otaArmed() const {
    return ota.isArmed();
}

bool MyBle::sleepAllowed() const {
    return !ota.isArmed() && !ota.isWriting();
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
    NimBLEDevice::init(deviceName.c_str());
    NimBLEDevice::setMTU(185);
    NimBLEDevice::setPower(3);
    // Use maximum power only for discovery/OTA advertising and connection
    // initiation. Once connected, the lower default power preserves battery.
    NimBLEDevice::setPower(9, NimBLETxPowerType::Advertise);
    NimBLEDevice::setPower(9, NimBLETxPowerType::Scan);
    peersMutex = xSemaphoreCreateMutex();
    CHECK_POINTER(peersMutex, ErrorCode::SEMAPHORE_CREATE_FAILED, "BLE peer mutex");

    server = NimBLEDevice::createServer();
    server->setCallbacks(new BleServerCallbacks(*this), true);
    server->advertiseOnDisconnect(true);
    NimBLEService* service = server->createService(kServiceUuid);

    NimBLECharacteristic* identity = service->createCharacteristic(
        kIdentityUuid, NIMBLE_PROPERTY::READ, sizeof(peerId));
    identity->setValue(peerId);
    NimBLECharacteristic* protocol = service->createCharacteristic(
        kProtocolUuid, NIMBLE_PROPERTY::READ, sizeof(scorebot::kWireProtocolVersion));
    protocol->setValue(scorebot::kWireProtocolVersion);
    uplink = service->createCharacteristic(
        kUplinkUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        scorebot::kMaxWireMessageSize);
    downlink = service->createCharacteristic(
        kDownlinkUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        scorebot::kMaxWireMessageSize);
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
        now - lastScanStartedMs < scanBackoffMs) {
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
        client->setConnectionParams(80, 100, 4, 600);
        client->setConnectTimeout(5000);
    }

    pendingConnectionClient.store(client);
    connectionAttemptStartedMs.store(millis());
    if (!client->connect(true, true, true)) {
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
    backOffPeer(pendingConnectionAddress);
    if (client != nullptr) {
        client->cancelConnect();
    }
}

void MyBle::addPeer(NimBLEClient* client) {
    const uint32_t discoveryStartedMs = millis();
    NimBLERemoteService* service = client->getService(kServiceUuid);
    DEBUG_PRINTF("BLE: service discovered in %lu ms\n",
                 static_cast<unsigned long>(millis() - discoveryStartedMs));
    keepExistingPeersAlive();
    if (service == nullptr) {
        DEBUG_PRINTLN("BLE: player service discovery failed");
        client->disconnect();
        return;
    }
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
        if (client->isConnected()) {
            backOffPeer(client->getPeerAddress());
        }
        client->disconnect();
        return;
    }

    const uint32_t nodeId = identity->readValue<uint32_t>();
    const uint16_t remoteProtocol = protocol->readValue<uint16_t>();
    const BoardRole role = getRoleConfig(nodeId).role;
    const uint8_t roleMask = isPlayerRole(role)
                                 ? static_cast<uint8_t>(1u << (static_cast<uint8_t>(role) -
                                                              static_cast<uint8_t>(BoardRole::Player_Red)))
                                 : 0;
    if (remoteProtocol != scorebot::kWireProtocolVersion || nodeId == 0 ||
        nodeId == peerId || !isPlayerRole(role) ||
        (rosterFrozen.load() && (rosterMask.load() & roleMask) == 0)) {
        DEBUG_PRINTF(
            "BLE: rejected player identity=%08lx protocol=%u expected=%u frozen=%d roster=0x%02x\n",
            static_cast<unsigned long>(nodeId), static_cast<unsigned>(remoteProtocol),
            static_cast<unsigned>(scorebot::kWireProtocolVersion), rosterFrozen.load(),
            static_cast<unsigned>(rosterMask.load()));
        backOffPeer(client->getPeerAddress());
        client->disconnect();
        return;
    }

    {
        ScopedSemaphore lock(peersMutex);
        if (peers.count(nodeId) != 0) {
            DEBUG_PRINTF("BLE: duplicate player identity %08lx\n", static_cast<unsigned long>(nodeId));
            client->disconnect();
            return;
        }
        peers.emplace(nodeId, Peer{nodeId, client, remoteDownlink, false});
    }
    if (!remoteUplink->subscribe(true,
        [this, nodeId](NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
            receive(nodeId, data, length, kNoConnection);
        })) {
        ScopedSemaphore lock(peersMutex);
        peers.erase(nodeId);
        client->disconnect();
        return;
    }
    connectionSettledMs.store(millis());
    finishPendingConnection(client);
    DEBUG_PRINTF("BLE: registered player %08lx\n", static_cast<unsigned long>(nodeId));
}

void MyBle::finishPendingConnection(NimBLEClient* client) {
    NimBLEClient* expected = client;
    if (client != nullptr && pendingConnectionClient.compare_exchange_strong(expected, nullptr)) {
        connectionPending.store(false);
        connectionAttemptStartedMs.store(0);
    }
}

void MyBle::removePeer(NimBLEClient* client) {
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
        if (pendingLostPeerCount < pendingLostPeers.size()) {
            pendingLostPeers[pendingLostPeerCount++] = nodeId;
        }
    }
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

void MyBle::backOffPeer(const NimBLEAddress& address) {
    ScopedSemaphore lock(peersMutex);
    rejectedPeers[nextRejectedPeer] = {address, millis() + kRejectedPeerBackoffMs};
    nextRejectedPeer = (nextRejectedPeer + 1) % rejectedPeers.size();
}

void MyBle::keepExistingPeersAlive() {
    if (coordinator->myRole() == BoardRole::Leader) {
        coordinator->state.heartbeat(coordinator);
    }
}

size_t MyBle::peerCount() const {
    ScopedSemaphore lock(peersMutex);
    return peers.size();
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

    for (size_t index = 0; index < newCount; ++index) {
        if (emit(EventType::NewPeer, newPeers[index])) {
            ScopedSemaphore lock(peersMutex);
            const auto peer = peers.find(newPeers[index]);
            if (peer != peers.end()) {
                peer->second.announced = true;
            }
        }
    }
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
}

bool MyBle::sendTo(uint32_t nodeId, const String& message) {
    if (message.length() > scorebot::kMaxWireMessageSize) {
        return false;
    }
    if (coordinator->myRole() != BoardRole::Leader) {
        const uint16_t connection = leaderConnectionHandle.load();
        if (!leaderConnected.load() || connection == kNoConnection || uplink == nullptr) {
            return false;
        }
        uplink->setValue(reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
        return uplink->notify(connection);
    }

    ScopedSemaphore lock(peersMutex);
    auto peer = peers.find(nodeId);
    return peer != peers.end() && peer->second.client->isConnected() &&
           peer->second.downlink->writeValue(
               reinterpret_cast<const uint8_t*>(message.c_str()), message.length(), false);
}

bool MyBle::sendBroadcast(const String& message) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return sendTo(getNodeIdForRole(BoardRole::Leader), message);
    }
    std::array<uint32_t, kPlayerCount> nodeIds{};
    size_t count = 0;
    {
        ScopedSemaphore lock(peersMutex);
        for (const auto& [nodeId, peer] : peers) {
            if (count < nodeIds.size()) {
                nodeIds[count++] = nodeId;
            }
        }
    }
    bool sent = false;
    for (size_t index = 0; index < count; ++index) {
        sent = sendTo(nodeIds[index], message) || sent;
    }
    return sent;
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
    if (!enqueue(event)) {
        ++droppedMessages;
    }
}

bool MyBle::enqueue(const Event& event) {
    return xQueueSend(coordinator->eventQueue, &event, 0) == pdPASS;
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
    ota.loop();
    const uint32_t now = millis();
    if (server != nullptr && now - lastAdvertisingCheckMs >= 1000) {
        lastAdvertisingCheckMs = now;
        NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
        const bool shouldAdvertise = coordinator->myRole() != BoardRole::Leader || ota.isArmed();
        if (shouldAdvertise && server->getConnectedCount() == 0 && !advertising->isAdvertising()) {
            advertising->start();
        } else if (!shouldAdvertise && coordinator->myRole() == BoardRole::Leader &&
                   advertising->isAdvertising()) {
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
