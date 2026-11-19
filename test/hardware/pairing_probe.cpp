#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <BoardIdentity.hpp>
#include <BleService.hpp>
#include <PairingRules.hpp>
#include <atomic>

namespace {
struct Reception {
    uint64_t address;
    uint8_t type;
    char bytes[scorebot::kMaxWireMessageSize + 1];
};
struct SeenLeader { scorebot::Discovery info; uint64_t address; uint8_t type; };
QueueHandle_t incoming, discoveries;
NimBLECharacteristic *metadata, *uplink;
scorebot::Discovery local{};
SeenLeader leader{};
std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE};
std::atomic<unsigned> connectionCount{0};
bool configured = false, finished = false, matched = false;
uint32_t configuredAt, stageStarted, lastRequest, game, term, version, duplicateVersion;
uint32_t targetSession = 0;
unsigned stage = 0;
String command;

void fail(const char* reason) {
    if (!finished) Serial.printf("PROBE_FAIL stage=%u %s\n", stage, reason);
    finished = true;
}
void advertise() {
    const auto packet = scorebot::encodeDiscovery(local);
    metadata->setValue(packet.data(), packet.size());
    NimBLEAdvertisementData data;
    data.setFlags(BLE_HS_ADV_F_BREDR_UNSUP | BLE_HS_ADV_F_DISC_GEN);
    data.setManufacturerData(packet.data(), packet.size());
    auto* adv = NimBLEDevice::getAdvertising();
    adv->setAdvertisementData(data);
    if (!adv->isAdvertising()) adv->start();
}
class Scanner final : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        for (uint8_t i = 0; i < device->getManufacturerDataCount(); ++i) {
            const auto data = device->getManufacturerData(i);
            SeenLeader seen{};
            if (!scorebot::decodeDiscovery(reinterpret_cast<const uint8_t*>(data.data()), data.size(), seen.info) ||
                !seen.info.authority()) continue;
            seen.address = static_cast<uint64_t>(device->getAddress());
            seen.type = device->getAddress().isPublic() ? 0 : 1;
            (void)xQueueSend(discoveries, &seen, 0);
        }
    }
} scanner;
class Server final : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
        connection.store(info.getConnHandle());
        connectionCount.fetch_add(1);
        Serial.println("PROBE_CONNECTED");
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        connection.store(BLE_HS_CONN_HANDLE_NONE);
    }
} serverCallbacks;
class Downlink final : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
        const auto value = characteristic->getValue();
        if (value.size() > scorebot::kMaxWireMessageSize) return;
        Reception reception{};
        reception.address = static_cast<uint64_t>(info.getAddress());
        reception.type = info.getAddress().isPublic() ? 0 : 1;
        memcpy(reception.bytes, value.data(), value.size());
        (void)xQueueSend(incoming, &reception, 0);
    }
} downlinkCallbacks;
void transmit() {
    if (connection.load() == BLE_HS_CONN_HANDLE_NONE) return;
    uplink->setValue(command.c_str());
    uplink->notify(connection.load());
    lastRequest = millis();
}
void sendControl(unsigned operation, unsigned kind, unsigned player, int32_t value, bool pass = false) {
    JsonDocument doc;
    doc["type"] = "control";
    doc["protocol"] = scorebot::kWireProtocolVersion;
    doc["from"] = local.node;
    doc["op"] = operation;
    doc["game"] = game;
    doc["term"] = term;
    doc["command"] = kind;
    doc["player"] = player;
    doc["value"] = value;
    doc["pass"] = pass;
    command = "";
    serializeJson(doc, command);
    stageStarted = millis();
    transmit();
}
void onState(const JsonDocument& doc) {
    game = doc["game"].as<uint32_t>();
    term = doc["term"].as<uint32_t>();
    version = doc["version"].as<uint32_t>();
    uint32_t acknowledged = 0;
    for (unsigned i = 0; i < 4; ++i) if (doc["controllers"][i].as<uint32_t>() == local.node)
        acknowledged = doc["controlOps"][i].as<uint32_t>();
    const int32_t red = doc["scores"][0].as<int32_t>();
    const int32_t blue = doc["scores"][1].as<int32_t>();
    const unsigned turn = doc["turn"].as<unsigned>();
    if (stage == 1) {
        if (doc["started"].as<bool>() || red != 0 || blue != 0 ||
            doc["scores"][2].as<int32_t>() != 0 || doc["scores"][3].as<int32_t>() != 0 ||
            doc["connected"].as<unsigned>() != 0 || doc["local"].as<unsigned>() != 0 ||
            doc["enabled"].as<unsigned>() != 0 || doc["explicit"].as<unsigned>() != 0 ||
            doc["roster"].as<unsigned>() != 0) { fail("requires a fresh lobby"); return; }
        stage = 2; sendControl(1, 2, 0, 0); // Red LOCAL.
    } else if (stage == 2 && acknowledged >= 1) {
        stage = 3; sendControl(2, 2, 1, 0); // Blue LOCAL.
    } else if (stage == 3 && acknowledged >= 2) {
        stage = 4; sendControl(3, 1, 0, 0); // Start.
    } else if (stage == 4 && acknowledged >= 3) {
        if (!doc["started"].as<bool>() || doc["roster"].as<unsigned>() != 3 || turn != 2) {
            fail("start/roster/turn"); return;
        }
        stage = 5; sendControl(4, 0, 0, 7); // Red +7.
    } else if (stage == 5 && acknowledged >= 4) {
        if (red != 7 || blue != 0 || turn != 2) { fail("score commit"); return; }
        duplicateVersion = version;
        stage = 6;
        stageStarted = millis();
        transmit(); // Same operation again.
    } else if (stage == 6) {
        if (red != 7 || version != duplicateVersion) { fail("duplicate changed state"); return; }
        if (millis() - stageStarted > 1200) {
            Serial.println("PROBE_DUPLICATE_PASS");
            stage = 7; sendControl(5, 0, 0, 0, true); // Red passes to Blue.
        }
    } else if (stage == 7 && acknowledged >= 5) {
        if (turn != 3 || red != 7) { fail("turn pass"); return; }
        stage = 8; sendControl(6, 0, 1, -2); // Blue correction.
    } else if (stage == 8 && acknowledged >= 6) {
        if (red != 7 || blue != -2 || turn != 3) { fail("blue correction"); return; }
        duplicateVersion = version;
        JsonDocument spoof;
        spoof["type"] = "player"; spoof["protocol"] = scorebot::kWireProtocolVersion;
        spoof["score"] = 999; spoof["turnPassed"] = true;
        spoof["fromNodeId"] = local.node; spoof["operationId"] = 99; spoof["game"] = game;
        command = ""; serializeJson(spoof, command);
        stage = 9; stageStarted = millis(); transmit();
    } else if (stage == 9 && millis() - stageStarted > 1200) {
        if (red != 7 || blue != -2 || turn != 3 || version != duplicateVersion) {
            fail("controller acquired player privileges"); return;
        }
        Serial.printf("PROBE_PASS leader=%08lx session=%08lx pair=%u version=%lu scores=[7,-2,0,0] turn=BLUE\n",
            static_cast<unsigned long>(local.leader), static_cast<unsigned long>(local.session), local.pairing,
            static_cast<unsigned long>(version));
        finished = true;
    }
}
}

void setup() {
    Serial.begin(115200);
    incoming = xQueueCreate(8, sizeof(Reception));
    discoveries = xQueueCreate(16, sizeof(SeenLeader));
    local.node = scorebot::boardIdFromEfuseMac(ESP.getEfuseMac());
    local.kind = scorebot::DeviceKind::Leaderboard;
    local.role = BoardRole::Leader;
    NimBLEDevice::init("Scorebot bench probe");
    NimBLEDevice::setMTU(185);
    NimBLEDevice::setPower(9);
    auto* server = NimBLEDevice::createServer();
    server->setCallbacks(&serverCallbacks, false);
    auto* service = server->createService(scorebot::kGameServiceUuid);
    service->createCharacteristic(scorebot::kIdentityUuid, NIMBLE_PROPERTY::READ)->setValue(local.node);
    service->createCharacteristic(scorebot::kProtocolUuid, NIMBLE_PROPERTY::READ)->setValue(scorebot::kWireProtocolVersion);
    metadata = service->createCharacteristic(scorebot::kPairingUuid, NIMBLE_PROPERTY::READ);
    uplink = service->createCharacteristic(scorebot::kUplinkUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
                                         scorebot::kMaxWireMessageSize);
    service->createCharacteristic(scorebot::kDownlinkUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
                                 scorebot::kMaxWireMessageSize)->setCallbacks(&downlinkCallbacks);
    server->start();
    auto* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanner, true);
    scan->setActiveScan(true); scan->setInterval(100); scan->setWindow(100); scan->setMaxResults(0);
    scan->start(0);
    Serial.println("PROBE_READY send JSON {leader,session,pair} over UART");
}
void loop() {
    static String input;
    while (Serial.available()) {
        const char ch = static_cast<char>(Serial.read());
        if (ch != '\n') { if (input.length() < 128) input += ch; continue; }
        JsonDocument doc;
        if (!configured && deserializeJson(doc, input) == DeserializationError::Ok &&
            doc["leader"].is<uint32_t>() && doc["session"].is<uint32_t>() && doc["pair"].is<uint8_t>() &&
            doc["leader"].as<uint32_t>() != 0 && doc["session"].as<uint32_t>() != 0 && doc["pair"].as<uint8_t>() <= 20) {
            local.leader = doc["leader"].as<uint32_t>();
            targetSession = doc["session"].as<uint32_t>();
            local.session = targetSession ^ 0x5a5a5a5au;
            if (local.session == 0) local.session = 1;
            local.pairing = doc["pair"].as<uint8_t>();
            local.flags = scorebot::Selected;
            configured = true; configuredAt = millis();
            advertise();
            Serial.println("PROBE_FOREIGN_SESSION_BEGIN");
        }
        input = "";
    }
    if (!configured || finished) { delay(10); return; }
    SeenLeader incomingLeader{};
    while (xQueueReceive(discoveries, &incomingLeader, 0) == pdPASS) {
        if (incomingLeader.info.node == local.leader && incomingLeader.info.session == targetSession) {
            leader = incomingLeader; matched = true;
        }
    }
    if (stage == 0 && millis() - configuredAt >= 6500) {
        if (connectionCount.load() != 0) fail("foreign session was connected");
        else if (!matched) fail("selected leader was not discovered");
        else {
            Serial.println("PROBE_FOREIGN_SESSION_PASS");
            local.session = targetSession;
            stage = 1; stageStarted = millis(); advertise();
        }
    }
    Reception reception{};
    while (xQueueReceive(incoming, &reception, 0) == pdPASS) {
        JsonDocument doc;
        if (reception.address != leader.address || reception.type != leader.type ||
            deserializeJson(doc, reception.bytes) != DeserializationError::Ok || doc["type"] != "state" ||
            doc["protocol"].as<uint16_t>() != scorebot::kWireProtocolVersion ||
            doc["leader"].as<uint32_t>() != local.leader || doc["session"].as<uint32_t>() != targetSession) continue;
        if (stage > 0 && !finished) onState(doc);
    }
    if (stage >= 2 && !finished && millis() - lastRequest > 1000) transmit();
    if (stage > 0 && !finished && millis() - stageStarted > 25000) fail("state/ack timeout");
    delay(10);
}
