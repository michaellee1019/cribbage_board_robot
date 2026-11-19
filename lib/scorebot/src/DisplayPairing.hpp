#pragma once

#include <PairingRules.hpp>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>

// Displays without controls follow their saved session, or the sole visible
// session after a full discovery window. Ambiguous rooms wait for selection;
// they never switch games because an unrelated advertisement arrived first.
class DisplayPairing {
    struct Seen {
        scorebot::Discovery info{};
        uint64_t address{0};
        uint8_t type{0};
        uint32_t received{0};
    };
    QueueHandle_t queue{nullptr};
    std::array<Seen, 16> seen{};
    scorebot::Discovery chosen{};
    uint32_t started{0};
    std::atomic<uint32_t> leader{0};
    std::atomic<uint32_t> session{0};
    std::atomic<uint64_t> address{0};
    std::atomic<uint8_t> addressType{0};
public:
    bool begin() {
        queue = xQueueCreate(16, sizeof(Seen));
        started = millis();
        Preferences preferences;
        if (preferences.begin("display-pair", true)) {
            chosen.leader = preferences.getUInt("leader", 0);
            chosen.session = preferences.getUInt("session", 0);
            chosen.pairing = preferences.getUChar("pair", scorebot::kNoPairingId);
            preferences.end();
        }
        return queue != nullptr;
    }
    void observe(const NimBLEAdvertisedDevice* device) {
        if (!queue) return;
        for (uint8_t i = 0; i < device->getManufacturerDataCount(); ++i) {
            const auto data = device->getManufacturerData(i);
            Seen incoming{};
            if (!scorebot::decodeDiscovery(reinterpret_cast<const uint8_t*>(data.data()), data.size(), incoming.info) ||
                !incoming.info.authority()) continue;
            incoming.address = static_cast<uint64_t>(device->getAddress());
            incoming.type = device->getAddress().isPublic() ? 0 : 1;
            incoming.received = millis();
            (void)xQueueSend(queue, &incoming, 0);
        }
    }
    bool service() {
        Seen incoming{};
        while (queue && xQueueReceive(queue, &incoming, 0) == pdPASS) {
            size_t slot = 0;
            for (size_t i = 0; i < seen.size(); ++i) {
                if (seen[i].info.node == incoming.info.node) { slot = i; break; }
                if (seen[i].received < seen[slot].received) slot = i;
            }
            seen[slot] = incoming;
        }
        if (millis() - started < scorebot::kElectionListenMs) return false;
        const Seen* candidate = nullptr;
        size_t candidates = 0;
        for (const auto& item : seen) {
            if (item.received == 0 || millis() - item.received > scorebot::kDiscoveryLifetimeMs) continue;
            if (chosen.hasSession()) {
                if (scorebot::sameSession(chosen, item.info)) candidate = &item;
            } else { candidate = &item; ++candidates; }
        }
        if (!candidate || (!chosen.hasSession() && candidates != 1)) return false;
        const bool changed = !scorebot::sameSession(chosen, candidate->info) || address.load() != candidate->address;
        chosen = candidate->info;
        addressType.store(candidate->type);
        address.store(candidate->address);
        session.store(chosen.session);
        leader.store(chosen.leader);
        if (changed) {
            Serial.printf("Pairing: following leader=%08lx session=%08lx pair=%u\n",
                static_cast<unsigned long>(chosen.leader), static_cast<unsigned long>(chosen.session), chosen.pairing);
            Preferences preferences;
            if (preferences.begin("display-pair", false)) {
                preferences.putUInt("leader", chosen.leader);
                preferences.putUInt("session", chosen.session);
                preferences.putUChar("pair", chosen.pairing);
                preferences.end();
            }
        }
        return changed;
    }
    scorebot::Discovery metadata(uint32_t node) const {
        return {node, chosen.leader, chosen.session, chosen.pairing, scorebot::DeviceKind::Display,
                BoardRole::Unknown, static_cast<uint8_t>(leader.load() ? scorebot::Selected : 0), 0};
    }
    bool accepts(const NimBLEAddress& incoming) const {
        return leader.load() != 0 && static_cast<uint64_t>(incoming) == address.load() &&
            (incoming.isPublic() ? 0 : 1) == addressType.load();
    }
    bool accepts(const NimBLEAdvertisedDevice* device) const {
        if (!accepts(device->getAddress())) return false;
        for (uint8_t i = 0; i < device->getManufacturerDataCount(); ++i) {
            const auto data = device->getManufacturerData(i);
            scorebot::Discovery info{};
            if (scorebot::decodeDiscovery(reinterpret_cast<const uint8_t*>(data.data()), data.size(), info) &&
                info.authority() && info.node == leader.load() && info.session == session.load()) return true;
        }
        return false;
    }
};
