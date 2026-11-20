#pragma once

#include <BoardRole.hpp>
#include <Protocol.hpp>
#include <array>
#include <cstddef>
#include <cstdint>

namespace scorebot {
constexpr uint8_t kMaxPairingId = 20;
constexpr uint8_t kNoPairingId = 0xff;
constexpr uint32_t kDiscoveryLifetimeMs = 12000;
constexpr uint32_t kElectionListenMs = 5000;
enum class DeviceKind : uint8_t { Player = 1, Leaderboard = 2, Display = 3 };
enum DiscoveryFlag : uint8_t { Authority = 1, Selected = 2, Recovering = 4 };

constexpr bool isPlayerRole(BoardRole role) {
    return role >= BoardRole::Player_Red && role <= BoardRole::Player_Yellow;
}
constexpr uint8_t roleMask(BoardRole role) {
    return isPlayerRole(role) ? static_cast<uint8_t>(1u <<
        (static_cast<unsigned>(role) - static_cast<unsigned>(BoardRole::Player_Red))) : 0;
}
constexpr bool validDisplayCount(uint8_t count) { return count == 1 || count == 4; }
constexpr DeviceKind kindForDisplayCount(uint8_t count) {
    return count == 4 ? DeviceKind::Leaderboard : DeviceKind::Player;
}

struct Discovery {
    uint32_t node{0};
    uint32_t leader{0};
    uint32_t session{0};
    uint8_t pairing{kNoPairingId};
    DeviceKind kind{DeviceKind::Player};
    BoardRole role{BoardRole::Unknown};
    uint8_t flags{0};
    uint8_t occupied{0};
    constexpr bool authority() const { return (flags & Authority) != 0; }
    constexpr bool selected() const { return (flags & Selected) != 0; }
    constexpr bool hasSession() const {
        return leader != 0 && session != 0 && pairing <= kMaxPairingId;
    }
};
constexpr bool validDiscovery(const Discovery& value) {
    if (value.node == 0 || (value.flags & ~7u) != 0 || (value.occupied & ~15u) != 0 ||
        (value.pairing != kNoPairingId && value.pairing > kMaxPairingId)) return false;
    if (value.kind == DeviceKind::Leaderboard) {
        if (value.role != BoardRole::Leader) return false;
    } else if (value.kind == DeviceKind::Player) {
        if (value.role != BoardRole::Unknown && !isPlayerRole(value.role)) return false;
    } else if (value.kind != DeviceKind::Display || value.role != BoardRole::Unknown) return false;
    if ((value.selected() || value.authority()) && !value.hasSession()) return false;
    if (value.selected() && value.kind == DeviceKind::Player && !isPlayerRole(value.role)) return false;
    return !value.authority() || (value.kind == DeviceKind::Leaderboard && value.leader == value.node);
}

// Manufacturer data fits a legacy primary advertisement alongside GAP flags.
// Multibyte fields are little endian, independently of compiler struct layout.
constexpr std::array<uint8_t, 5> kDiscoveryHeader{0xff, 0xff, 'S', 'P', kWireProtocolVersion};
using DiscoveryPacket = std::array<uint8_t, 22>;
inline DiscoveryPacket encodeDiscovery(const Discovery& value) {
    DiscoveryPacket packet{};
    for (size_t i = 0; i < kDiscoveryHeader.size(); ++i) packet[i] = kDiscoveryHeader[i];
    size_t out = kDiscoveryHeader.size();
    for (uint32_t number : {value.node, value.leader, value.session}) {
        for (unsigned byte = 0; byte < 4; ++byte) packet[out++] = static_cast<uint8_t>(number >> (8 * byte));
    }
    packet[out++] = value.pairing;
    packet[out++] = static_cast<uint8_t>(value.kind);
    packet[out++] = static_cast<uint8_t>(value.role);
    packet[out++] = value.flags;
    packet[out] = value.occupied;
    return packet;
}
inline bool decodeDiscovery(const uint8_t* bytes, size_t size, Discovery& value) {
    if (!bytes || size != DiscoveryPacket{}.size()) return false;
    for (size_t i = 0; i < kDiscoveryHeader.size(); ++i) if (bytes[i] != kDiscoveryHeader[i]) return false;
    Discovery copy{};
    size_t in = kDiscoveryHeader.size();
    for (uint32_t* number : {&copy.node, &copy.leader, &copy.session}) {
        *number = 0;
        for (unsigned byte = 0; byte < 4; ++byte) *number |= static_cast<uint32_t>(bytes[in++]) << (8 * byte);
    }
    copy.pairing = bytes[in++];
    copy.kind = static_cast<DeviceKind>(bytes[in++]);
    copy.role = static_cast<BoardRole>(bytes[in++]);
    copy.flags = bytes[in++];
    copy.occupied = bytes[in];
    if (!validDiscovery(copy)) return false;
    value = copy;
    return true;
}
constexpr bool sameSession(const Discovery& a, const Discovery& b) {
    return a.hasSession() && b.hasSession() && a.leader == b.leader &&
           a.session == b.session;
}
// Existing authorities win startup elections; simultaneous candidates use
// their runtime IDs. Established games never merge on subsequent scans.
constexpr bool preferredStartupLeader(const Discovery& candidate, const Discovery& current) {
    if (candidate.kind != DeviceKind::Leaderboard || !validDiscovery(candidate) || candidate.leader != candidate.node) return false;
    if (current.kind != DeviceKind::Leaderboard || !validDiscovery(current)) return true;
    if (candidate.authority() != current.authority()) return candidate.authority();
    if ((candidate.flags & Recovering) != (current.flags & Recovering)) return (candidate.flags & Recovering) != 0;
    return candidate.node < current.node;
}
inline uint8_t choosePairingId(uint32_t random, uint32_t usedMask) {
    const uint8_t first = static_cast<uint8_t>(random % (kMaxPairingId + 1));
    for (uint8_t i = 0; i <= kMaxPairingId; ++i) {
        const auto id = static_cast<uint8_t>((first + i) % (kMaxPairingId + 1));
        if ((usedMask & (1u << id)) == 0) return id;
    }
    return kNoPairingId;
}

enum class PeerKind { Rejected, Player, Display, Controller };
struct PeerAdmission {
    PeerKind kind{PeerKind::Rejected};
    uint8_t playerMask{0};
    constexpr bool eligibleForFrozenRoster(uint8_t mask) const {
        return kind == PeerKind::Display || kind == PeerKind::Controller ||
               (kind == PeerKind::Player && (playerMask & mask) != 0);
    }
};
inline PeerAdmission classifyPeerAdmission(const Discovery& local, const Discovery& remote) {
    if (!validDiscovery(local) || !local.authority() || !validDiscovery(remote) ||
        remote.node == local.node || remote.authority() || !remote.selected() || !sameSession(local, remote)) return {};
    if (remote.kind == DeviceKind::Display) return {PeerKind::Display, 0};
    if (remote.kind == DeviceKind::Leaderboard) return {PeerKind::Controller, 0};
    return {PeerKind::Player, roleMask(remote.role)};
}
// Each directory belongs to one game. Addresses are learned from transport,
// never from a payload or factory allowlist. Disconnected colors remain bound
// for sleeping players, so another board cannot take over an occupied slot.
class PeerDirectory {
public:
    struct Entry {
        uint32_t node{0};
        uint64_t address{0};
        uint8_t addressType{0};
        BoardRole role{BoardRole::Unknown};
        PeerKind kind{PeerKind::Rejected};
    };
    std::array<Entry, 9> entries{};
    const Entry* find(uint32_t node) const {
        for (const auto& entry : entries) if (entry.node != 0 && entry.node == node) return &entry;
        return nullptr;
    }
    uint8_t occupied() const {
        uint8_t mask = 0;
        for (const auto& entry : entries) if (entry.kind == PeerKind::Player) mask |= roleMask(entry.role);
        return mask;
    }
    PeerAdmission admit(const Discovery& local, const Discovery& remote, uint64_t address, uint8_t addressType) {
        const auto admission = classifyPeerAdmission(local, remote);
        if (admission.kind == PeerKind::Rejected || address == 0 || address > 0xffffffffffffULL || addressType > 1) return {};
        Entry* available = nullptr;
        size_t sameKind = 0;
        for (auto& entry : entries) {
            if (entry.kind == admission.kind) ++sameKind;
            if (entry.node == remote.node) {
                return entry.address == address && entry.addressType == addressType && entry.role == remote.role &&
                       entry.kind == admission.kind ? admission : PeerAdmission{};
            }
            if (entry.node != 0 && entry.address == address && entry.addressType == addressType) return {};
            if (admission.kind == PeerKind::Player && entry.kind == PeerKind::Player && entry.role == remote.role) return {};
            if (entry.node == 0 && !available) available = &entry;
        }
        if (!available || (admission.kind == PeerKind::Controller && sameKind >= 4) ||
            (admission.kind == PeerKind::Display && sameKind >= 1)) return {};
        *available = {remote.node, address, addressType, remote.role, admission.kind};
        return admission;
    }
};
}
