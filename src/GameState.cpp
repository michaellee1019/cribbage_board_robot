#include <Coordinator.hpp>
#include <GameState.hpp>
#include <GameRules.hpp>
#include <Messages.hpp>
#include <Protocol.hpp>
#include <ReplicationRules.hpp>
#include <utils.hpp>

#include <Preferences.h>

#include <algorithm>
#include <array>

namespace {
constexpr char kStateNamespace[] = "scorebot";
constexpr char kStateKey[] = "state";
constexpr uint32_t kReplicationPeriodMs = 1500;
constexpr uint32_t kOperationRetryMs = 1000;
constexpr uint32_t kOtaArmHoldMs = 3000;

constexpr BoardRole kPlayers[] = {
    BoardRole::Player_Red,
    BoardRole::Player_Blue,
    BoardRole::Player_Green,
    BoardRole::Player_White,
};

size_t roleIndex(BoardRole role) {
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        if (kPlayers[index] == role) {
            return index;
        }
    }
    return std::size(kPlayers);
}

bool isPlayer(BoardRole role) {
    return roleIndex(role) < std::size(kPlayers);
}

int32_t& scoreFor(GameState& state, BoardRole role) {
    return state.scores.at(roleIndex(role));
}

const int32_t& scoreFor(const GameState& state, BoardRole role) {
    return state.scores.at(roleIndex(role));
}

uint32_t& operationFor(GameState& state, BoardRole role) {
    return state.lastOperation.at(roleIndex(role));
}

const uint32_t& operationFor(const GameState& state, BoardRole role) {
    return state.lastOperation.at(roleIndex(role));
}

scorebot::Player toRulePlayer(BoardRole role) {
    const size_t index = roleIndex(role);
    return index < std::size(kPlayers) ? static_cast<scorebot::Player>(index) : scorebot::Player::None;
}

BoardRole fromRulePlayer(scorebot::Player player) {
    return scorebot::isPlayer(player) ? kPlayers[scorebot::playerIndex(player)] : BoardRole::Unknown;
}

scorebot::Snapshot toRules(const GameState& state) {
    scorebot::Snapshot rules{};
    rules.started = state.gameStarted;
    rules.turn = toRulePlayer(state.whosTurn);
    rules.version = state.version;
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        rules.scores[index] = scoreFor(state, role);
        rules.lastOperation[index] = operationFor(state, role);
    }
    rules.connectedMask = state.connectedMask;
    return rules;
}

void fromRules(GameState& state, const scorebot::Snapshot& rules) {
    state.gameStarted = rules.started;
    state.whosTurn = fromRulePlayer(rules.turn);
    state.version = rules.version;
    state.connectedMask = rules.connectedMask;
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        scoreFor(state, role) = rules.scores[index];
        operationFor(state, role) = rules.lastOperation[index];
    }
}

String snapshotJson(const GameState& state) {
    JsonDocument document;
    document["type"] = "state";
    document["protocol"] = scorebot::kWireProtocolVersion;
    document["term"] = state.term;
    document["version"] = state.version;
    document["leader"] = state.leaderId;
    document["started"] = state.gameStarted;
    document["turn"] = static_cast<uint8_t>(state.whosTurn);

    JsonArray scores = document["scores"].to<JsonArray>();
    JsonArray operations = document["operations"].to<JsonArray>();
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        scores.add(scoreFor(state, role));
        operations.add(operationFor(state, role));
    }
    document["connected"] = state.connectedMask;

    String encoded;
    serializeJson(document, encoded);
    return encoded;
}

enum class SnapshotDecodeResult { Invalid, Older, Equal, Applied };

SnapshotDecodeResult decodeSnapshot(GameState& state, const String& json) {
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "state" ||
        !document["protocol"].is<uint16_t>() ||
        document["protocol"].as<uint16_t>() != scorebot::kWireProtocolVersion ||
        !document["term"].is<uint32_t>() || !document["version"].is<uint32_t>() ||
        !document["leader"].is<uint32_t>() || !document["scores"].is<JsonArray>() ||
        !document["operations"].is<JsonArray>()) {
        return SnapshotDecodeResult::Invalid;
    }

    const uint32_t term = document["term"].as<uint32_t>();
    const uint32_t version = document["version"].as<uint32_t>();
    if (document["leader"].as<uint32_t>() != getNodeIdForRole(BoardRole::Leader)) {
        return SnapshotDecodeResult::Invalid;
    }
    JsonArray scores = document["scores"].as<JsonArray>();
    JsonArray operations = document["operations"].as<JsonArray>();
    if (scores.size() != std::size(kPlayers) || operations.size() != std::size(kPlayers)) {
        return SnapshotDecodeResult::Invalid;
    }
    const uint32_t connectedMask = document["connected"] | 0;
    const BoardRole turn = static_cast<BoardRole>(document["turn"] | 0);
    if ((connectedMask & ~0x0fu) != 0 ||
        (turn != BoardRole::Unknown && !isPlayer(turn))) {
        return SnapshotDecodeResult::Invalid;
    }
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        if (!scores[index].is<int32_t>() || !operations[index].is<uint32_t>()) {
            return SnapshotDecodeResult::Invalid;
        }
    }
    switch (scorebot::compareRevision({term, version}, {state.term, state.version})) {
        case scorebot::RevisionOrder::Older:
            return SnapshotDecodeResult::Older;
        case scorebot::RevisionOrder::Equal:
            return SnapshotDecodeResult::Equal;
        case scorebot::RevisionOrder::Newer:
            break;
    }

    state.term = term;
    state.version = version;
    state.leaderId = document["leader"].as<uint32_t>();
    state.gameStarted = document["started"] | false;
    state.whosTurn = turn;
    state.connectedMask = static_cast<uint8_t>(connectedMask);
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        scoreFor(state, role) = scores[index].as<int32_t>();
        operationFor(state, role) = operations[index].as<uint32_t>();
    }
    state.leaderless = false;
    return SnapshotDecodeResult::Applied;
}

void setPlayerDisplay(const GameState& state, Coordinator* coordinator) {
    if (coordinator->myRole() == BoardRole::Leader) {
        return;
    }
    if (state.leaderless) {
        coordinator->display1.print("dEGr");
        coordinator->rotaryEncoder.setColor(0x000000);
    } else if (state.gameStarted && state.whosTurn == coordinator->myRole()) {
        coordinator->display1.print("BEEF");
        coordinator->rotaryEncoder.setColor(coordinator->myRoleConfig()->color);
    } else if (state.myScore == 0) {
        coordinator->display1.print("----");
        coordinator->rotaryEncoder.setColor(0x000000);
    } else {
        coordinator->display1.print(strFormat("%d", state.myScore));
    }
}

void setLeaderboardDisplays(const GameState& state, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const std::array<HT16Display*, 4> displays = {
        &coordinator->display1, &coordinator->display2, &coordinator->display3, &coordinator->display4};
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        displays[index]->print(strFormat("%d", scoreFor(state, kPlayers[index])));
    }
}

void replicate(const GameState& state, Coordinator* coordinator) {
    coordinator->ble.sendBroadcast(snapshotJson(state));
}

void commit(GameState& state, Coordinator* coordinator) {
    state.leaderId = coordinator->ble.getMyPeerId();
    state.leaderless = false;
    if (!state.persist()) {
        FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "leader game-state commit");
        return;
    }
    state.refreshDisplays(coordinator);
    replicate(state, coordinator);
}

void rejectAndResync(const GameState& state, Coordinator* coordinator, uint32_t sender) {
    coordinator->ble.sendTo(sender, snapshotJson(state));
}

void sendPlayerOperation(GameState* state, Coordinator* coordinator, bool passesTurn) {
    if (!state->gameStarted || !coordinator->ble.hasLeader() || state->leaderless ||
        state->pendingOperation != 0) {
        setPlayerDisplay(*state, coordinator);
        return;
    }
    if (passesTurn && state->whosTurn != coordinator->myRole()) {
        return;
    }
    const uint32_t operationId = state->nextOperationId();
    PlayerMessage message(state->myScore, passesTurn, coordinator->ble.getMyPeerId(), operationId);
    const String encoded = message.toJson();
    if (coordinator->ble.sendTo(getNodeIdForRole(BoardRole::Leader), encoded)) {
        state->pendingOperation = operationId;
        state->pendingScore = state->myScore;
        state->pendingPass = passesTurn;
        state->pendingMessage = encoded;
        state->pendingSentMs = millis();
        coordinator->display1.print("WAIT");
    }
}

void onButtonPress(GameState* state, const ButtonPressEvent& event, Coordinator* coordinator) {
    const BoardRole role = coordinator->myRole();
    if (event.buttonName == ButtonName::RotaryEncoder) {
        if (coordinator->rotaryEncoder.pressed()) {
            if (state->rotaryPressStartedMs == 0) {
                state->rotaryPressStartedMs = millis();
            }
            return;
        }

        if (state->rotaryPressStartedMs != 0) {
            const uint32_t heldMs = millis() - state->rotaryPressStartedMs;
            state->rotaryPressStartedMs = 0;
            if (heldMs >= kOtaArmHoldMs) {
                coordinator->ble.armOta();
                coordinator->display1.print("OTA ");
                coordinator->rotaryEncoder.setColor(0x004040);
            } else if (role != BoardRole::Leader && state->pendingOperation == 0) {
                // A short rotary press is the existing quick-score action. It
                // remains legal off-turn so an accidental score can be fixed.
                sendPlayerOperation(state, coordinator, false);
            }
            return;
        }

        if (role == BoardRole::Leader || state->pendingOperation != 0) {
            return;
        }
        const int32_t delta = coordinator->rotaryEncoder.delta();
        if (delta != 0 && !state->leaderless) {
            state->myScore += delta;
            setPlayerDisplay(*state, coordinator);
        }
        return;
    }

    const ButtonGrid::Interrupt interrupt = coordinator->buttonGrid.consumeInterrupt();
    const uint8_t pin = interrupt.pin;
    const uint16_t value = interrupt.captured;
    if (value != ButtonGrid::intValReleased && value != ButtonGrid::intValReleased2) {
        return;
    }

    if (role == BoardRole::Leader) {
        scorebot::Snapshot rules = toRules(*state);
        if (pin == 0 && scorebot::start(rules)) {
            fromRules(*state, rules);
            commit(*state, coordinator);
        }
        return;
    }

    if (state->leaderless || state->pendingOperation != 0) {
        setPlayerDisplay(*state, coordinator);
        return;
    }
    if (pin == ButtonGrid::add) {
        if (state->myScore != 0) {
            sendPlayerOperation(state, coordinator, false);
        }
    } else if (pin == ButtonGrid::negone) {
        --state->myScore;
    } else if (pin == ButtonGrid::plusone) {
        ++state->myScore;
    } else if (pin == ButtonGrid::plusfive) {
        state->myScore += 5;
    } else if (pin == ButtonGrid::okPin) {
        sendPlayerOperation(state, coordinator, true);
        return;
    }
    setPlayerDisplay(*state, coordinator);
}

void onPlayerMessage(GameState* state, const Event& event, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const String json = event.messageReceived.message;
    if (!PlayerMessage::isPlayerMessage(json)) {
        return;
    }
    const PlayerMessage message = PlayerMessage::fromJson(json);
    const uint32_t sender = event.messageReceived.peerId;
    const BoardRoleConfig senderConfig = getRoleConfig(sender);
    if (!isPlayer(senderConfig.role) || message.fromNodeId != sender || !state->gameStarted) {
        rejectAndResync(*state, coordinator, sender);
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    const scorebot::ApplyResult result = scorebot::apply(
        rules, {toRulePlayer(senderConfig.role), message.score, message.turnPassed, message.operationId});
    if (result != scorebot::ApplyResult::Accepted) {
        rejectAndResync(*state, coordinator, sender);
        return;
    }
    fromRules(*state, rules);
    commit(*state, coordinator);
}

void onNewPeer(GameState* state, const Event& event, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    if (scorebot::connect(rules, toRulePlayer(getRoleConfig(event.newPeer.peerId).role))) {
        fromRules(*state, rules);
        commit(*state, coordinator);
    } else {
        rejectAndResync(*state, coordinator, event.newPeer.peerId);
    }
}

void onLostPeer(GameState* state, const Event& event, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    if (scorebot::disconnect(rules, toRulePlayer(getRoleConfig(event.lostPeer.peerId).role))) {
        fromRules(*state, rules);
        commit(*state, coordinator);
    }
}
}  // namespace

GameState::GameState()
    : myScore(0),
      whosTurn(BoardRole::Unknown),
      scores{},
      connectedMask(0),
      gameStarted(false),
      term(0),
      version(0),
      leaderId(0),
      leaderless(true),
      lastOperation{},
      localOperation(0),
      lastReplicationMs(0),
      pendingOperation(0),
      pendingScore(0),
      pendingPass(false),
      pendingMessage(),
      pendingSentMs(0),
      rotaryPressStartedMs(0) {}

void GameState::restore() {
    Preferences preferences;
    if (!preferences.begin(kStateNamespace, true)) {
        return;
    }
    const String saved = preferences.getString(kStateKey, "");
    preferences.end();
    if (!saved.isEmpty()) {
        decodeSnapshot(*this, saved);
    }
}

bool GameState::persist() const {
    Preferences preferences;
    if (!preferences.begin(kStateNamespace, false)) {
        return false;
    }
    const String encoded = snapshotJson(*this);
    const bool stored = preferences.putString(kStateKey, encoded) == encoded.length();
    preferences.end();
    return stored;
}

bool GameState::hasLeader() const {
    return !leaderless && leaderId != 0;
}

uint32_t GameState::nextOperationId() {
    return ++localOperation;
}

void GameState::refreshDisplays(Coordinator* coordinator) const {
    setLeaderboardDisplays(*this, coordinator);
    setPlayerDisplay(*this, coordinator);
}

void GameState::heartbeat(Coordinator* coordinator) {
    const uint32_t now = millis();
    if (coordinator->myRole() != BoardRole::Leader) {
        // A notification can be received while the leader is briefly busy. Keep
        // the original operation id and retry until a newer snapshot commits it.
        if (pendingOperation != 0 && coordinator->ble.hasLeader() &&
            now - pendingSentMs >= kOperationRetryMs && !pendingMessage.isEmpty()) {
            if (coordinator->ble.sendTo(getNodeIdForRole(BoardRole::Leader), pendingMessage)) {
                pendingSentMs = now;
            }
        }
        return;
    }
    if (now - lastReplicationMs >= kReplicationPeriodMs) {
        leaderId = coordinator->ble.getMyPeerId();
        leaderless = false;
        replicate(*this, coordinator);
        lastReplicationMs = now;
    }
}

void GameState::handleEvent(const Event& event, Coordinator* coordinator) {
    switch (event.type) {
        case EventType::ButtonPressed:
            onButtonPress(this, event.press, coordinator);
            break;
        case EventType::BleConnected:
            leaderless = false;
            break;
        case EventType::BleLeaderLost:
            if (coordinator->myRole() != BoardRole::Leader) {
                leaderless = true;
                refreshDisplays(coordinator);
            }
            break;
        case EventType::NewPeer:
            onNewPeer(this, event, coordinator);
            break;
        case EventType::LostPeer:
            onLostPeer(this, event, coordinator);
            break;
        case EventType::MessageReceived: {
            const String json = event.messageReceived.message;
            if (PlayerMessage::isPlayerMessage(json)) {
                onPlayerMessage(this, event, coordinator);
            } else {
                const SnapshotDecodeResult decoded = decodeSnapshot(*this, json);
                if (decoded == SnapshotDecodeResult::Invalid ||
                    decoded == SnapshotDecodeResult::Older) {
                    break;
                }
                const BoardRole myRole = coordinator->myRole();
                if (isPlayer(myRole)) {
                    coordinator->ble.confirmLeader(event.messageReceived.connectionHandle);
                    localOperation = scorebot::reconcileOperationId(
                        localOperation, operationFor(*this, myRole));
                }
                if (decoded == SnapshotDecodeResult::Equal) {
                    // A rebooted player commonly receives only equal revision
                    // heartbeats. They prove the fixed leaderboard is live.
                    leaderless = false;
                    refreshDisplays(coordinator);
                    break;
                }
                if (isPlayer(myRole)) {
                    if (pendingOperation != 0 && operationFor(*this, myRole) >= pendingOperation) {
                        myScore = 0;
                        coordinator->rotaryEncoder.reset();
                        pendingOperation = 0;
                        pendingScore = 0;
                        pendingPass = false;
                        pendingMessage = "";
                        pendingSentMs = 0;
                    }
                }
                if (!persist()) {
                    FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "player state replication");
                }
                refreshDisplays(coordinator);
            }
            break;
        }
        case EventType::StateUpdate:
            break;
    }
}
