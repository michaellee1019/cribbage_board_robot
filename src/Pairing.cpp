#include <MyBle.hpp>
#include <Coordinator.hpp>
#include <DisplayBrightness.hpp>
#include <ErrorHandler.hpp>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>
#include <utils.hpp>
#include <algorithm>

namespace {
class PairingLock {
public:
    explicit PairingLock(SemaphoreHandle_t mutex) : mutex(mutex) { xSemaphoreTake(mutex, portMAX_DELAY); }
    ~PairingLock() { xSemaphoreGive(mutex); }
private:
    SemaphoreHandle_t mutex;
};
uint32_t randomIdentity() {
    uint32_t value;
    do { value = esp_random(); } while (value == 0);
    return value;
}
}

scorebot::Discovery MyBle::discovery() const {
    PairingLock lock(peersMutex);
    auto value = localDiscovery;
    value.occupied = directory.occupied();
    return value;
}
uint32_t MyBle::sessionId() const { return discovery().session; }
uint8_t MyBle::pairingId() const { return discovery().pairing; }
BoardRole MyBle::peerRole(uint32_t node) const {
    if (node != 0 && node == authorityNodeId()) return BoardRole::Leader;
    PairingLock lock(peersMutex);
    const auto* entry = directory.find(node);
    return entry ? entry->role : BoardRole::Unknown;
}
bool MyBle::isControllerPeer(uint32_t node) const {
    PairingLock lock(peersMutex);
    const auto peer = peers.find(node);
    return peer != peers.end() && peer->second.controller;
}
bool MyBle::acceptsLeader(uint32_t node) const {
    return !isAuthority() && !pairingUiActive() && node != 0 && node == authorityNodeId();
}
uint32_t MyBle::senderForConnection(const NimBLEConnInfo& connection) const {
    if (isAuthority() || pairingUiActive()) return 0;
    PairingLock lock(peersMutex);
    return leaderAddressKnown && connection.getAddress() == selectedLeaderAddress ? localDiscovery.leader : 0;
}
void MyBle::rotatePairing(int32_t delta) { pairingRotation.fetch_add(delta); }
void MyBle::confirmPairing() { pairingConfirmed.store(true); }
void MyBle::chooseColor() { colorRequested.store(true); }
void MyBle::clearPairing() {
    Preferences preferences;
    if (preferences.begin("scorebot-pair", false)) {
        if (isAuthority()) preferences.remove("peers");
        else preferences.clear();
        preferences.end();
    }
}
void MyBle::initializePairing() {
    Preferences preferences;
    String savedPeers;
    bool saved = preferences.begin("scorebot-pair", true);
    const uint8_t color = saved ? preferences.getUChar("color", 0) : 0;
    const auto preset = suggestedRole(peerId);
    const BoardRole savedRole = static_cast<BoardRole>(color);
    suggestedColor = scorebot::isPlayerRole(savedRole) ? savedRole :
        scorebot::isPlayerRole(preset) ? preset : BoardRole::Player_Red;
    localDiscovery.role = configuredRole.load() == BoardRole::Leader ? BoardRole::Leader :
        scorebot::isPlayerRole(savedRole) ? savedRole : preset;
    configuredRole.store(localDiscovery.role);
    if (saved) {
        localDiscovery.leader = preferences.getUInt("leader", 0);
        localDiscovery.session = preferences.getUInt("session", 0);
        localDiscovery.pairing = preferences.getUChar("pair", scorebot::kNoPairingId);
        saved = preferences.getBool("paired", false);
        savedPeers = preferences.getString("peers", "");
        preferences.end();
    }
    savedSelection = saved && localDiscovery.hasSession();
    const bool leaderboard = localDiscovery.kind == scorebot::DeviceKind::Leaderboard;
    if (leaderboard && (!savedSelection || localDiscovery.leader == peerId)) {
        localDiscovery.leader = peerId;
        if (!savedSelection) {
            localDiscovery.session = randomIdentity();
            localDiscovery.pairing = static_cast<uint8_t>(esp_random() % 21);
        }
        bool recoveringGame = false;
        if (savedSelection) {
            Preferences statePreferences;
            if (statePreferences.begin("scorebot", true)) {
                const String snapshot = statePreferences.getString("state", "");
                statePreferences.end();
                JsonDocument state;
                recoveringGame = deserializeJson(state, snapshot) == DeserializationError::Ok &&
                    state["protocol"].as<uint16_t>() == scorebot::kWireProtocolVersion &&
                    state["leader"].as<uint32_t>() == peerId &&
                    state["session"].as<uint32_t>() == localDiscovery.session &&
                    state["started"].is<bool>() && state["started"].as<bool>();
            }
        }
        localDiscovery.flags = recoveringGame ? scorebot::Recovering : 0;
    }
    if (leaderboard && savedSelection && localDiscovery.leader == peerId && !savedPeers.isEmpty()) {
        JsonDocument document;
        scorebot::PeerDirectory restored;
        auto authority = localDiscovery;
        authority.flags = scorebot::Authority | scorebot::Selected;
        bool valid = deserializeJson(document, savedPeers) == DeserializationError::Ok && document.is<JsonArray>();
        for (JsonObject entry : document.as<JsonArray>()) {
            if (!entry["node"].is<uint32_t>() || !entry["address"].is<uint64_t>() ||
                !entry["type"].is<uint8_t>() || !entry["role"].is<uint8_t>() || !entry["kind"].is<uint8_t>()) { valid = false; break; }
            const auto role = static_cast<BoardRole>(entry["role"].as<uint8_t>());
            const auto kind = static_cast<scorebot::PeerKind>(entry["kind"].as<uint8_t>());
            const auto deviceKind = kind == scorebot::PeerKind::Controller ? scorebot::DeviceKind::Leaderboard :
                kind == scorebot::PeerKind::Player ? scorebot::DeviceKind::Player : scorebot::DeviceKind::Display;
            auto remote = authority;
            remote.node = entry["node"].as<uint32_t>();
            remote.role = role;
            remote.kind = deviceKind;
            remote.flags = scorebot::Selected;
            if (restored.admit(authority, remote, entry["address"].as<uint64_t>(), entry["type"].as<uint8_t>()).kind != kind) {
                valid = false; break;
            }
        }
        if (valid) directory = restored;
    }
    choosingColor = !leaderboard && !scorebot::isPlayerRole(localDiscovery.role);
    pairingPrompt.store(!leaderboard);
}
void MyBle::savePairing() {
    const auto value = discovery();
    JsonDocument peersJson;
    JsonArray entries = peersJson.to<JsonArray>();
    {
        PairingLock lock(peersMutex);
        for (const auto& peer : directory.entries) {
            if (peer.node == 0) continue;
            JsonObject entry = entries.add<JsonObject>();
            entry["node"] = peer.node; entry["address"] = peer.address;
            entry["type"] = peer.addressType; entry["role"] = static_cast<uint8_t>(peer.role);
            entry["kind"] = static_cast<uint8_t>(peer.kind);
        }
    }
    String savedPeers;
    serializeJson(peersJson, savedPeers);
    Preferences preferences;
    if (!preferences.begin("scorebot-pair", false)) {
        FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "pairing settings");
    }
    const bool stored = preferences.putBool("paired", value.selected()) != 0 &&
        preferences.putUChar("color", static_cast<uint8_t>(value.role)) != 0 &&
        preferences.putUInt("leader", value.leader) != 0 &&
        preferences.putUInt("session", value.session) != 0 &&
        preferences.putUChar("pair", value.pairing) != 0;
    const bool peersStored = preferences.putString("peers", savedPeers) == savedPeers.length();
    preferences.end();
    if (!stored || !peersStored) FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "save pairing settings");
    savedSelection = value.selected() && value.hasSession();
}
void MyBle::observe(const NimBLEAdvertisedDevice* device) {
    if (!discoveryQueue || device->getAddress() == NimBLEDevice::getAddress()) return;
    for (uint8_t i = 0; i < device->getManufacturerDataCount(); ++i) {
        const std::string data = device->getManufacturerData(i);
        SeenBoard seen{};
        if (scorebot::decodeDiscovery(reinterpret_cast<const uint8_t*>(data.data()), data.size(), seen.info)) {
            seen.address = device->getAddress();
            seen.seenAtMs = millis();
            (void)xQueueSend(discoveryQueue, &seen, 0);
        }
    }
}
void MyBle::processDiscoveries() {
    SeenBoard incoming{};
    while (xQueueReceive(discoveryQueue, &incoming, 0) == pdPASS) {
        if (incoming.info.node == peerId) continue;
        size_t slot = seenBoards.size();
        size_t oldest = 0;
        for (size_t i = 0; i < seenBoards.size(); ++i) {
            const auto& item = seenBoards[i];
            if (item.info.node == incoming.info.node) { slot = i; break; }
            if (item.seenAtMs == 0) slot = i;
            if (item.seenAtMs < seenBoards[oldest].seenAtMs) oldest = i;
        }
        if (slot == seenBoards.size()) slot = oldest;
        seenBoards[slot] = incoming;
        const auto local = discovery();
        if (incoming.info.authority() && scorebot::sameSession(local, incoming.info)) {
            PairingLock lock(peersMutex);
            selectedLeaderAddress = incoming.address;
            leaderAddressKnown = true;
            localDiscovery.pairing = incoming.info.pairing;
        }
        if (!startupDiscovery && !isAuthority() && localRole() != BoardRole::Leader && !hasLeader() &&
            incoming.info.authority() && incoming.info.node == authorityNodeId() &&
            incoming.info.session != local.session) {
            pairingPrompt.store(true);
            { PairingLock lock(peersMutex); localDiscovery.flags = 0; }
            savedSelection = false;
        }
        if (!startupDiscovery && isAuthority() && incoming.info.authority() &&
            !scorebot::sameSession(local, incoming.info) && incoming.info.pairing == local.pairing && peerId > incoming.info.node) {
            uint32_t used = 0;
            for (const auto& item : seenBoards) if (item.info.authority() && item.seenAtMs != 0 &&
                millis() - item.seenAtMs <= scorebot::kDiscoveryLifetimeMs) used |= 1u << item.info.pairing;
            const uint8_t replacement = scorebot::choosePairingId(esp_random(), used);
            if (replacement != scorebot::kNoPairingId) {
                { PairingLock lock(peersMutex); localDiscovery.pairing = replacement; }
                savePairing();
            }
        }
        if (startupDiscovery || !isAuthority() || connectionPending.load() || peerIsBackedOff(incoming.address)) continue;
        const auto admission = scorebot::classifyPeerAdmission(local, incoming.info);
        if (admission.kind == scorebot::PeerKind::Rejected ||
            (rosterFrozen.load() && !admission.eligibleForFrozenRoster(rosterMask.load()))) continue;
        bool eligible = false;
        {
            PairingLock lock(peersMutex);
            // Test on a copy; connecting or subscribing can fail without
            // occupying a color. Final binding occurs after GATT validation.
            auto proposed = directory;
            eligible = proposed.admit(local, incoming.info, static_cast<uint64_t>(incoming.address),
                incoming.address.isPublic() ? 0 : 1).kind != scorebot::PeerKind::Rejected;
            if (peers.count(incoming.info.node) != 0) eligible = false;
        }
        if (!eligible || connectionPending.exchange(true)) continue;
        pendingConnectionAddress = incoming.address;
        connectionAttemptStartedMs.store(millis());
        connectionRequested.store(true);
    }
}
void MyBle::finishStartupDiscovery() {
    const uint32_t started = millis();
    while (millis() - started < scorebot::kElectionListenMs) {
        processDiscoveries();
        coordinator->setStatusLight(renderPairingUi(millis()));
        delay(10);
    }
    NimBLEDevice::getScan()->stop();
    processDiscoveries();
    if (localDiscovery.kind == scorebot::DeviceKind::Leaderboard) {
        auto preferred = localDiscovery;
        const SeenBoard* selected = nullptr;
        uint32_t occupied = 0;
        for (const auto& item : seenBoards) {
            if (item.seenAtMs == 0 || millis() - item.seenAtMs > scorebot::kDiscoveryLifetimeMs ||
                item.info.kind != scorebot::DeviceKind::Leaderboard) continue;
            if (item.info.authority()) occupied |= 1u << item.info.pairing;
            // A follower recovering its saved game never creates a competing
            // authority just because its leader happens to be asleep.
            const bool pinned = (localDiscovery.flags & scorebot::Recovering) != 0 ||
                                (savedSelection && localDiscovery.leader != peerId);
            if (pinned && (item.info.node != localDiscovery.leader || item.info.session != localDiscovery.session)) continue;
            if (scorebot::preferredStartupLeader(item.info, preferred)) { preferred = item.info; selected = &item; }
        }
        if (savedSelection && localDiscovery.leader != peerId && !selected) {
            localIsAuthority.store(false);
            localDiscovery.flags = scorebot::Selected;
        } else if (selected) {
            if (localDiscovery.leader != preferred.node || localDiscovery.session != preferred.session) directory = {};
            localDiscovery.leader = preferred.node;
            localDiscovery.session = preferred.session;
            localDiscovery.pairing = preferred.pairing;
            localDiscovery.flags = scorebot::Selected;
            selectedLeaderAddress = selected->address;
            leaderAddressKnown = true;
            localIsAuthority.store(false);
        } else {
            // A retained recovery session keeps its number. A fresh election
            // avoids all occupied numbers and detects the fully occupied case.
            if (!savedSelection && (occupied & (1u << localDiscovery.pairing)) != 0)
                localDiscovery.pairing = scorebot::choosePairingId(esp_random(), occupied);
            if (localDiscovery.pairing == scorebot::kNoPairingId) {
                FATAL_ERROR(ErrorCode::BLE_INIT_FAILED, "all pairing numbers in use");
            }
            localDiscovery.leader = peerId;
            localDiscovery.flags = scorebot::Authority | scorebot::Selected;
            localIsAuthority.store(true);
        }
        selectedAuthority.store(localDiscovery.leader);
        pairingPrompt.store(false);
        savePairing();
    } else {
        selectedAuthority.store(localDiscovery.leader);
        const auto wake = esp_sleep_get_wakeup_cause();
        if (savedSelection && scorebot::isPlayerRole(localDiscovery.role) && wake != ESP_SLEEP_WAKEUP_UNDEFINED) {
            localDiscovery.flags = scorebot::Selected;
            pairingPrompt.store(false);
        }
    }
    DEBUG_PRINTF("Pairing: node=%08lx leader=%08lx session=%08lx pair=%u authority=%d role=%u confirmed=%d\n",
        static_cast<unsigned long>(peerId), static_cast<unsigned long>(localDiscovery.leader),
        static_cast<unsigned long>(localDiscovery.session), static_cast<unsigned>(localDiscovery.pairing),
        isAuthority(), static_cast<unsigned>(localRole()), localDiscovery.selected());
    startupDiscovery = false;
    lastScanStartedMs = 0;
    configureAdvertising(false);
    updateAdvertising(false);
    beginScan();
}
void MyBle::servicePairing() {
    if (colorRequested.exchange(false) && !isAuthority() && localRole() != BoardRole::Leader) {
        choosingColor = true;
        pairingPrompt.store(true);
        { PairingLock lock(peersMutex); localDiscovery.flags = 0; }
        savedSelection = false;
        leaderConnected.store(false);
        leaderConnectionHandle.store(BLE_HS_CONN_HANDLE_NONE);
        lastPairingFrame = 0xffffffff;
    }
    if (!pairingUiActive()) return;
    std::array<size_t, 16> sessions{};
    size_t count = 0;
    for (size_t i = 0; i < seenBoards.size(); ++i) {
        const auto& item = seenBoards[i];
        if (item.info.authority() && item.seenAtMs != 0 && millis() - item.seenAtMs <= scorebot::kDiscoveryLifetimeMs)
            sessions[count++] = i;
    }
    std::sort(sessions.begin(), sessions.begin() + count, [this](size_t a, size_t b) {
        const auto& x = seenBoards[a].info;
        const auto& y = seenBoards[b].info;
        return x.pairing == y.pairing ? x.node < y.node : x.pairing < y.pairing;
    });
    if (selectedSessionIndex >= count) selectedSessionIndex = 0;
    int32_t rotation = pairingRotation.exchange(0);
    if (rotation != 0) lastPairingFrame = 0xffffffff;
    if (choosingColor) {
        const int index = static_cast<int>(suggestedColor) - static_cast<int>(BoardRole::Player_Red);
        suggestedColor = static_cast<BoardRole>(static_cast<int>(BoardRole::Player_Red) + ((index + rotation % 4 + 4) % 4));
    } else if (count != 0) {
        if (rotation == 0 && savedSelection) {
            for (size_t i = 0; i < count; ++i) if (scorebot::sameSession(localDiscovery, seenBoards[sessions[i]].info)) selectedSessionIndex = i;
        }
        if (rotation != 0) {
            selectedSessionIndex = (static_cast<int>(selectedSessionIndex) + rotation % static_cast<int>(count) + count) % count;
            savedSelection = false;
        }
    }
    if (!pairingConfirmed.exchange(false)) return;
    if (choosingColor) {
        configuredRole.store(suggestedColor);
        {
            PairingLock lock(peersMutex);
            localDiscovery.role = suggestedColor;
            localDiscovery.flags = 0;
        }
        choosingColor = false;
        lastPairingFrame = 0xffffffff;
        savePairing();
        return;
    }
    if (count == 0) return; // Only discovered sessions can be confirmed.
    const auto& selected = seenBoards[sessions[selectedSessionIndex]];
    const bool reconnecting = savedSelection && scorebot::sameSession(localDiscovery, selected.info);
    if (!reconnecting && (selected.info.occupied & scorebot::roleMask(localRole())) != 0) {
        choosingColor = true;
        lastPairingFrame = 0xffffffff;
        return;
    }
    {
        PairingLock lock(peersMutex);
        localDiscovery.leader = selected.info.node;
        localDiscovery.session = selected.info.session;
        localDiscovery.pairing = selected.info.pairing;
        localDiscovery.flags = scorebot::Selected;
        selectedLeaderAddress = selected.address;
        leaderAddressKnown = true;
    }
    selectedAuthority.store(selected.info.node);
    savePairing();
    coordinator->resetReplicaForPairing();
    pairingPrompt.store(false);
    lastPairingFrame = 0xffffffff;
    configureAdvertising(false);
    updateAdvertising(false);
}
void MyBle::updatePairingAdvertisement() {
    const auto packet = scorebot::encodeDiscovery(discovery());
    if (pairingCharacteristic) pairingCharacteristic->setValue(packet.data(), packet.size());
    if (packet == advertisedDiscovery) return;
    NimBLEAdvertisementData data;
    data.setFlags(BLE_HS_ADV_F_BREDR_UNSUP | BLE_HS_ADV_F_DISC_GEN);
    data.setManufacturerData(packet.data(), packet.size());
    auto* advertising = NimBLEDevice::getAdvertising();
    if (advertising->setAdvertisementData(data)) advertisedDiscovery = packet;
}
uint32_t MyBle::renderPairingUi(uint32_t now) {
    const uint32_t frame = now / 700;
    const uint32_t pairingColor = choosingColor ? getRoleStyle(suggestedColor).color : 0x002020;
    const uint32_t displayedColor = frame % 2 ? pairingColor : 0;
    if (frame == lastPairingFrame) return displayedColor;
    lastPairingFrame = frame;
    String text = "SCAN";
    if (choosingColor) {
        text = frame % 2 ? getRoleStyle(suggestedColor).name : "COLR";
    } else {
        uint8_t pairing = localDiscovery.pairing;
        if (pairingUiActive()) {
            std::array<const SeenBoard*, 16> sessions{};
            size_t count = 0;
            for (const auto& item : seenBoards) if (item.info.authority() && item.seenAtMs != 0 &&
                now - item.seenAtMs <= scorebot::kDiscoveryLifetimeMs) sessions[count++] = &item;
            std::sort(sessions.begin(), sessions.begin() + count, [](const SeenBoard* a, const SeenBoard* b) {
                return a->info.pairing == b->info.pairing ? a->info.node < b->info.node : a->info.pairing < b->info.pairing;
            });
            if (count != 0) pairing = sessions[selectedSessionIndex < count ? selectedSessionIndex : 0]->info.pairing;
            else pairing = scorebot::kNoPairingId;
        }
        if (pairing <= scorebot::kMaxPairingId) text = frame % 2 ? strFormat("P %02u", pairing) : "PAIR";
    }
    coordinator->display1.setBrightnessNow(display_brightness::kActiveBrightness);
    coordinator->display1.print(text);
    if (localDiscovery.kind == scorebot::DeviceKind::Leaderboard) {
        coordinator->display2.print(text);
        coordinator->display3.print(text);
        coordinator->display4.print(text);
    }
    return displayedColor;
}
