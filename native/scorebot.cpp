#include "scorebot.h"

#include <GameRules.hpp>
#include <LeaderboardControlRules.hpp>
#include <MessageAuthorityRules.hpp>
#include <PeerAdmissionRules.hpp>
#include <ReplicationRules.hpp>

#include <new>

struct scorebot_game {
    scorebot::Snapshot rules{};
    uint32_t gameId{0};
    uint32_t term{0};
};

namespace {
using scorebot::Player;

scorebot_result changed(bool value) { return value ? SCOREBOT_OK : SCOREBOT_NO_CHANGE; }
bool validPlayer(uint32_t player) { return player < SCOREBOT_PLAYER_COUNT; }

bool validState(const scorebot_state_v1& state) {
    const uint32_t masks = state.connected_mask | state.local_control_mask |
        state.lobby_enabled_mask | state.lobby_explicit_mask | state.sleeping_mask | state.roster_mask;
    if ((masks & ~0x0fu) != 0 || state.started > 1 || state.turn > SCOREBOT_NO_PLAYER ||
        (state.local_control_mask & ~state.lobby_enabled_mask) != 0) {
        return false;
    }
    if (!state.started) {
        return state.turn == SCOREBOT_NO_PLAYER && state.roster_mask == 0;
    }
    return state.roster_mask != 0 && state.turn < SCOREBOT_NO_PLAYER &&
        (state.roster_mask & (1u << state.turn)) != 0 &&
        ((state.local_control_mask | state.connected_mask | state.sleeping_mask) & ~state.roster_mask) == 0 &&
        (state.roster_mask & ~state.lobby_enabled_mask) == 0;
}

bool validAddressArguments(uint32_t protocol, uint32_t publicAddress, uint64_t mac, uint32_t offset) {
    return protocol <= UINT16_MAX && publicAddress <= 1 && mac <= 0xffffffffffffULL &&
        (offset == 1 || offset == 2);
}

scorebot_result applyResult(scorebot::ApplyResult result) {
    switch (result) {
        case scorebot::ApplyResult::Accepted: return SCOREBOT_OK;
        case scorebot::ApplyResult::AcceptedWithoutTurnChange: return SCOREBOT_ACCEPTED_WITHOUT_TURN_CHANGE;
        case scorebot::ApplyResult::GameNotStarted: return SCOREBOT_GAME_NOT_STARTED;
        case scorebot::ApplyResult::UnknownPlayer: return SCOREBOT_INVALID_ARGUMENT;
        case scorebot::ApplyResult::NotConnected: return SCOREBOT_NOT_CONNECTED;
        case scorebot::ApplyResult::Duplicate: return SCOREBOT_DUPLICATE;
        case scorebot::ApplyResult::ScoreOutOfRange: return SCOREBOT_SCORE_OUT_OF_RANGE;
    }
    return SCOREBOT_REJECTED;
}
}  // namespace

extern "C" {
uint32_t scorebot_abi_version(void) { return SCOREBOT_ABI_VERSION; }
uint32_t scorebot_v1_state_size(void) { return sizeof(scorebot_state_v1); }
uint32_t scorebot_v1_protocol_version(void) { return scorebot::kWireProtocolVersion; }
uint32_t scorebot_v1_leader_node_id(void) { return getNodeIdForRole(BoardRole::Leader); }

scorebot_result scorebot_v1_create(uint32_t abiVersion, scorebot_game** out) {
    if (!out) return SCOREBOT_INVALID_ARGUMENT;
    *out = nullptr;
    if (abiVersion != SCOREBOT_ABI_VERSION) return SCOREBOT_ABI_MISMATCH;
    // This is the only allocating operation. All member constructors are
    // noexcept; nothrow allocation returns NULL rather than throwing.
    *out = new (std::nothrow) scorebot_game{};
    return *out ? SCOREBOT_OK : SCOREBOT_NO_MEMORY;
}

void scorebot_v1_destroy(scorebot_game* game) { delete game; }

scorebot_result scorebot_v1_read(const scorebot_game* game, scorebot_state_v1* out, uint32_t size) {
    if (!game || !out) return SCOREBOT_INVALID_ARGUMENT;
    if (size != sizeof(*out)) return SCOREBOT_ABI_MISMATCH;
    scorebot_state_v1 copy{};
    copy.abi_version = SCOREBOT_ABI_VERSION;
    for (uint32_t i = 0; i < SCOREBOT_PLAYER_COUNT; ++i) {
        copy.scores[i] = game->rules.scores[i];
        copy.last_operation[i] = game->rules.lastOperation[i];
    }
    copy.game_id = game->gameId;
    copy.term = game->term;
    copy.version = game->rules.version;
    copy.leader_id = scorebot_v1_leader_node_id();
    copy.connected_mask = game->rules.connectedMask;
    copy.local_control_mask = game->rules.localControlMask;
    copy.lobby_enabled_mask = game->rules.lobbyEnabledMask;
    copy.lobby_explicit_mask = game->rules.lobbyExplicitMask;
    copy.sleeping_mask = game->rules.sleepingMask;
    copy.roster_mask = game->rules.rosterMask;
    copy.turn = static_cast<uint32_t>(game->rules.turn);
    copy.started = game->rules.started;
    *out = copy;
    return SCOREBOT_OK;
}

scorebot_result scorebot_v1_restore(scorebot_game* game, const scorebot_state_v1* state, uint32_t size) {
    if (!game || !state) return SCOREBOT_INVALID_ARGUMENT;
    if (size != sizeof(*state) || state->abi_version != SCOREBOT_ABI_VERSION) return SCOREBOT_ABI_MISMATCH;
    if (state->leader_id != scorebot_v1_leader_node_id()) return SCOREBOT_WRONG_AUTHORITY;
    if (!validState(*state)) return SCOREBOT_INVALID_ARGUMENT;
    scorebot_game copy{};
    for (uint32_t i = 0; i < SCOREBOT_PLAYER_COUNT; ++i) {
        copy.rules.scores[i] = state->scores[i];
        copy.rules.lastOperation[i] = state->last_operation[i];
    }
    copy.gameId = state->game_id;
    copy.term = state->term;
    copy.rules.version = state->version;
    copy.rules.connectedMask = static_cast<uint8_t>(state->connected_mask);
    copy.rules.localControlMask = static_cast<uint8_t>(state->local_control_mask);
    copy.rules.lobbyEnabledMask = static_cast<uint8_t>(state->lobby_enabled_mask);
    copy.rules.lobbyExplicitMask = static_cast<uint8_t>(state->lobby_explicit_mask);
    copy.rules.sleepingMask = static_cast<uint8_t>(state->sleeping_mask);
    copy.rules.rosterMask = static_cast<uint8_t>(state->roster_mask);
    copy.rules.turn = static_cast<Player>(state->turn);
    copy.rules.started = state->started != 0;
    *game = copy;
    return SCOREBOT_OK;
}

scorebot_result scorebot_v1_resume_authority(scorebot_game* game) {
    if (!game) return SCOREBOT_INVALID_ARGUMENT;
    // Refuse epoch exhaustion rather than advertise an older authority term.
    if (game->term == UINT32_MAX) return SCOREBOT_REJECTED;
    scorebot::resumeAuthority(game->rules, game->term);
    return SCOREBOT_OK;
}

scorebot_result scorebot_v1_set_lobby_mode(scorebot_game* game, uint32_t player, uint32_t mode) {
    if (!game || !validPlayer(player) || mode > SCOREBOT_OFF) return SCOREBOT_INVALID_ARGUMENT;
    return changed(scorebot::setLobbyMode(game->rules, static_cast<Player>(player), static_cast<scorebot::LobbyMode>(mode)));
}
scorebot_result scorebot_v1_start(scorebot_game* game) {
    if (!game) return SCOREBOT_INVALID_ARGUMENT;
    return changed(scorebot::start(game->rules));
}
scorebot_result scorebot_v1_reset(scorebot_game* game) {
    if (!game) return SCOREBOT_INVALID_ARGUMENT;
    return changed(scorebot::resetGame(game->rules));
}
scorebot_result scorebot_v1_set_connected(scorebot_game* game, uint32_t player, uint32_t connected) {
    if (!game || !validPlayer(player) || connected > 1) return SCOREBOT_INVALID_ARGUMENT;
    const auto target = static_cast<Player>(player);
    if (!connected) return changed(scorebot::disconnect(game->rules, target));
    const bool woke = scorebot::clearSleeping(game->rules, target);
    const bool linked = scorebot::connect(game->rules, target);
    return changed(woke || linked);
}
scorebot_result scorebot_v1_mark_sleeping(scorebot_game* game, uint32_t player) {
    if (!game || !validPlayer(player)) return SCOREBOT_INVALID_ARGUMENT;
    return changed(scorebot::markSleeping(game->rules, static_cast<Player>(player)));
}
scorebot_result scorebot_v1_local_action(scorebot_game* game, uint32_t player, int32_t delta, uint32_t action) {
    if (!game || !validPlayer(player) || action > SCOREBOT_OK_ACTION) return SCOREBOT_INVALID_ARGUMENT;
    std::array<int32_t, scorebot::kPlayerCount> deltas{};
    deltas[player] = delta;
    const auto result = scorebot::applyLeaderboardAction(game->rules, deltas, static_cast<Player>(player),
        static_cast<scorebot::LeaderboardAction>(action));
    switch (result) {
        case scorebot::LeaderboardActionResult::Accepted: return SCOREBOT_OK;
        case scorebot::LeaderboardActionResult::NoOp: return SCOREBOT_NO_CHANGE;
        case scorebot::LeaderboardActionResult::Rejected: return SCOREBOT_REJECTED;
    }
    return SCOREBOT_REJECTED;
}
scorebot_result scorebot_v1_player_action(scorebot_game* game, uint32_t transportNode,
    uint32_t claimedNode, uint32_t gameId, int32_t delta, uint32_t passesTurn, uint32_t operationId) {
    if (!game || passesTurn > 1 || operationId == 0) return SCOREBOT_INVALID_ARGUMENT;
    const auto role = getRoleConfig(transportNode).role;
    if (transportNode != claimedNode || !scorebot::isPlayerRole(role)) return SCOREBOT_WRONG_AUTHORITY;
    if (gameId != game->gameId) return SCOREBOT_WRONG_GAME;
    const auto player = static_cast<Player>(static_cast<uint32_t>(role) - static_cast<uint32_t>(BoardRole::Player_Red));
    return applyResult(scorebot::apply(game->rules, {player, delta, passesTurn != 0, operationId}));
}
scorebot_result scorebot_v1_admit_peer(uint32_t node, uint32_t protocol, uint32_t publicAddress,
    uint64_t mac, uint32_t offset, uint32_t* outKind, uint32_t* outPlayer) {
    if (!outKind || !outPlayer || outKind == outPlayer || !validAddressArguments(protocol, publicAddress, mac, offset))
        return SCOREBOT_INVALID_ARGUMENT;
    const auto admission = scorebot::classifyPeerAdmission(node, static_cast<uint16_t>(protocol),
        publicAddress != 0, mac, static_cast<uint8_t>(offset), scorebot_v1_leader_node_id());
    *outKind = static_cast<uint32_t>(admission.kind);
    *outPlayer = admission.kind == scorebot::PeerKind::Player
        ? static_cast<uint32_t>(getRoleConfig(node).role) - static_cast<uint32_t>(BoardRole::Player_Red)
        : SCOREBOT_NO_PLAYER;
    return SCOREBOT_OK;
}
scorebot_result scorebot_v1_is_leader(uint32_t node, uint32_t protocol, uint32_t publicAddress,
    uint64_t mac, uint32_t offset, uint32_t* outTrusted) {
    if (!outTrusted || !validAddressArguments(protocol, publicAddress, mac, offset)) return SCOREBOT_INVALID_ARGUMENT;
    *outTrusted = scorebot::isConfiguredLeaderPeer(node, static_cast<uint16_t>(protocol),
        publicAddress != 0, mac, static_cast<uint8_t>(offset));
    return SCOREBOT_OK;
}
int32_t scorebot_v1_compare_revision(uint32_t it, uint32_t iv, uint32_t ct, uint32_t cv) {
    switch (scorebot::compareRevision({it, iv}, {ct, cv})) {
        case scorebot::RevisionOrder::Older: return -1;
        case scorebot::RevisionOrder::Equal: return 0;
        case scorebot::RevisionOrder::Newer: return 1;
    }
    return 0;
}
uint32_t scorebot_v1_reconcile_operation(uint32_t local, uint32_t replicated) {
    return scorebot::reconcileOperationId(local, replicated);
}
uint32_t scorebot_v1_operation_acknowledged(uint32_t pending, uint32_t replicated) {
    return scorebot::operationAcknowledged(pending, replicated);
}
}  // extern "C"
