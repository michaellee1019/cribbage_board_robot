#include <MyBle.hpp>

#include <BoardRole.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>
#include <Protocol.hpp>

#include <array>

constexpr char kServiceUuid[] = "c6a8619e-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kIdentityUuid[] = "c6a8619f-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kUplinkUuid[] = "c6a861a0-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kDownlinkUuid[] = "c6a861a1-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kProtocolUuid[] = "c6a861a2-2f9d-46bc-9a23-bb9c89a519be";
constexpr uint32_t kLeaderTimeoutMs = 6000;
constexpr uint16_t kMaxMessageSize = 240;
constexpr uint32_t kScanDurationMs = 2000;
constexpr uint32_t kScanBackoffMs = 5000;
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

    void onConnect(NimBLEServer*, NimBLEConnInfo&) override {}

    void onDisconnect(NimBLEServer*, NimBLEConnInfo& connection, int) override {
        if (owner.coordinator->myRole() != BoardRole::Leader &&
            owner.leaderConnectionHandle.load() == connection.getConnHandle()) {
            owner.leaderConnectionHandle.store(kNoConnection);
            owner.leaderConnected.store(false);
            owner.emit(EventType::BleLeaderLost);
        }
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
        NimBLEDevice::getScan()->stop();

        const bool newClient = client == nullptr;
        if (newClient) {
            client = NimBLEDevice::createClient();
            if (client == nullptr) {
                return;
            }
            client->setConnectionParams(80, 100, 4, 600);
            client->setConnectTimeout(5000);
        }
        // NimBLE's scan callback runs on its host task. Connecting here must
        // be asynchronous; service discovery is deferred to MyBle::loop.
        if (!client->connect(device->getAddress(), true, true, true)) {
            if (newClient) {
                NimBLEDevice::deleteClient(client);
            }
            return;
        }
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
      ota(),
      peers(),
      peersMutex(nullptr),
      lastLeaderActivityMs(0),
      leaderConnectionHandle(kNoConnection),
      leaderConnected(false),
      lastScanStartedMs(0) {}

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

void MyBle::confirmLeader(uint16_t connectionHandle) {
    if (coordinator->myRole() == BoardRole::Leader || connectionHandle == kNoConnection) {
        return;
    }
    leaderConnectionHandle.store(connectionHandle);
    lastLeaderActivityMs.store(millis());
    leaderConnected.store(true);
}

void MyBle::armOta() {
    ota.arm();
}

bool MyBle::otaArmed() const {
    return ota.isArmed();
}

void MyBle::setup() {
    // Keep the existing low-32-bit ESP MAC identity so the installed boards
    // retain their role assignments after the transport change.
    peerId = static_cast<uint32_t>(ESP.getEfuseMac());
    NimBLEDevice::init((String("Scorebot-") + String(peerId, HEX)).c_str());
    NimBLEDevice::setMTU(185);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);  // close-range tabletop use
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
        kUplinkUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, kMaxMessageSize);
    downlink = service->createCharacteristic(
        kDownlinkUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, kMaxMessageSize);
    downlinkCallbacks = std::make_unique<BleDownlinkCallbacks>(*this);
    downlink->setCallbacks(downlinkCallbacks.get());
    ota.setup(server);
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData advertisement;
    advertisement.setFlags(0x06);
    advertisement.addServiceUUID(kServiceUuid);
    advertising->setAdvertisementData(advertisement);
    advertising->setAdvertisingInterval(1600);  // 1 s while waiting for the leaderboard
    advertising->start();

    if (coordinator->myRole() == BoardRole::Leader) {
        scanCallbacks = std::make_unique<BleScanCallbacks>(*this);
        beginScan();
    }
}

void MyBle::beginScan() {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    NimBLEScan* scan = NimBLEDevice::getScan();
    const uint32_t now = millis();
    if (scan->isScanning() || peerCount() >= kPlayerCount ||
        now - lastScanStartedMs < kScanBackoffMs) {
        return;
    }
    scan->setScanCallbacks(scanCallbacks.get(), false);
    scan->setActiveScan(false);
    scan->setInterval(100);
    scan->setWindow(30);
    scan->setMaxResults(0);
    scan->start(kScanDurationMs, false, true);
    lastScanStartedMs = now;
}

void MyBle::addPeer(NimBLEClient* client) {
    NimBLERemoteService* service = client->getService(kServiceUuid);
    if (service == nullptr) {
        client->disconnect();
        return;
    }
    NimBLERemoteCharacteristic* identity = service->getCharacteristic(kIdentityUuid);
    NimBLERemoteCharacteristic* protocol = service->getCharacteristic(kProtocolUuid);
    NimBLERemoteCharacteristic* remoteUplink = service->getCharacteristic(kUplinkUuid);
    NimBLERemoteCharacteristic* remoteDownlink = service->getCharacteristic(kDownlinkUuid);
    if (identity == nullptr || protocol == nullptr || remoteUplink == nullptr || remoteDownlink == nullptr) {
        client->disconnect();
        return;
    }

    const uint32_t nodeId = identity->readValue<uint32_t>();
    if (protocol->readValue<uint16_t>() != scorebot::kWireProtocolVersion ||
        nodeId == 0 || nodeId == peerId || !isPlayerRole(getRoleConfig(nodeId).role)) {
        client->disconnect();
        return;
    }

    {
        ScopedSemaphore lock(peersMutex);
        if (peers.count(nodeId) != 0) {
            client->disconnect();
            return;
        }
        peers.emplace(nodeId, Peer{nodeId, client, remoteDownlink});
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
    emit(EventType::NewPeer, nodeId);
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
        emit(EventType::LostPeer, nodeId);
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
}

bool MyBle::sendTo(uint32_t nodeId, const String& message) {
    if (message.length() > kMaxMessageSize) {
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
               reinterpret_cast<const uint8_t*>(message.c_str()), message.length(), true);
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
    xQueueSend(coordinator->eventQueue, &event, 0);
}

void MyBle::emit(EventType type, uint32_t nodeId) {
    Event event{};
    event.type = type;
    if (type == EventType::NewPeer) {
        event.newPeer.peerId = nodeId;
    } else if (type == EventType::LostPeer) {
        event.lostPeer.peerId = nodeId;
    }
    xQueueSend(coordinator->eventQueue, &event, 0);
}

void MyBle::loop() {
    ota.loop();
    if (coordinator->myRole() == BoardRole::Leader) {
        reconcilePeers();
        beginScan();
        return;
    }

    const uint32_t now = millis();
    if (leaderConnected.load() && now - lastLeaderActivityMs.load() > kLeaderTimeoutMs) {
        leaderConnected.store(false);
        leaderConnectionHandle.store(kNoConnection);
        emit(EventType::BleLeaderLost);
    }
}
