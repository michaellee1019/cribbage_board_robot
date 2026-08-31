#include <Coordinator.hpp>
#include <ButtonInputRules.hpp>
#include <ErrorHandler.hpp>
#include <GameState.hpp>
#include <GameRules.hpp>
#include <LeaderboardUiRules.hpp>
#include <LightColorRules.hpp>
#include <Messages.hpp>
#include <OtaTransferRules.hpp>
#include <PlayerUiRules.hpp>
#include <Protocol.hpp>
#include <ReplicationRules.hpp>
#include <SleepRules.hpp>
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
    rules.rosterMask = state.rosterMask;
    return rules;
}

void fromRules(GameState& state, const scorebot::Snapshot& rules) {
    state.gameStarted = rules.started;
    state.whosTurn = fromRulePlayer(rules.turn);
    state.version = rules.version;
    state.connectedMask = rules.connectedMask;
    state.rosterMask = rules.rosterMask;
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
    document["game"] = state.gameId;
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
    document["roster"] = state.rosterMask;

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
        !document["game"].is<uint32_t>() || !document["term"].is<uint32_t>() ||
        !document["version"].is<uint32_t>() ||
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
    const bool rosterPresent = document["roster"].is<uint32_t>();
    const uint32_t rosterMask = rosterPresent ? document["roster"].as<uint32_t>() : 0;
    // Older snapshots did not distinguish a frozen roster from live links.
    // Resume those in the lobby instead of accidentally locking out a board.
    const bool gameStarted = (document["started"] | false) && rosterPresent;
    const BoardRole turn = static_cast<BoardRole>(document["turn"] | 0);
    if ((connectedMask & ~0x0fu) != 0 || (rosterMask & ~0x0fu) != 0 ||
        ((connectedMask & ~rosterMask) != 0 && gameStarted) ||
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
    state.gameId = document["game"].as<uint32_t>();
    state.version = version;
    state.leaderId = document["leader"].as<uint32_t>();
    state.gameStarted = gameStarted;
    state.whosTurn = gameStarted ? turn : BoardRole::Unknown;
    state.connectedMask = static_cast<uint8_t>(connectedMask);
    state.rosterMask = static_cast<uint8_t>(rosterMask);
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
    const bool isMyTurn = scorebot::playerTurnLocallyActive(
        state.whosTurn == coordinator->myRole(), state.pendingPass);
    const scorebot::PlayerDisplayMode mode = scorebot::playerDisplayMode(
        state.myScore, state.leaderless, state.gameStarted, isMyTurn);
    coordinator->setPlayerTurnAnimation(mode == scorebot::PlayerDisplayMode::Turn);

    switch (mode) {
        case scorebot::PlayerDisplayMode::Delta:
            coordinator->display1.print(strFormat("%d", state.myScore));
            break;
        case scorebot::PlayerDisplayMode::Pairing:
            coordinator->display1.print("PAIR");
            break;
        case scorebot::PlayerDisplayMode::Turn:
            coordinator->display1.print("GO  ");
            break;
        case scorebot::PlayerDisplayMode::Idle:
            coordinator->display1.print("----");
            break;
    }
}

void setLeaderboardDisplays(const GameState& state, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const std::array<HT16Display*, 4> displays = {
        &coordinator->display1, &coordinator->display2, &coordinator->display3, &coordinator->display4};
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        const bool connected = (state.connectedMask & (1u << index)) != 0;
        const bool inRoster = (state.rosterMask & (1u << index)) != 0;
        switch (scorebot::leaderboardDisplayMode(state.gameStarted, connected, inRoster)) {
            case scorebot::LeaderboardDisplayMode::Score:
                displays[index]->print(strFormat("%d", scoreFor(state, role)));
                break;
            case scorebot::LeaderboardDisplayMode::LobbyName:
                displays[index]->print(getRoleConfig(getNodeIdForRole(role)).name.c_str());
                break;
            case scorebot::LeaderboardDisplayMode::Pairing:
                displays[index]->print("PAIR");
                break;
            case scorebot::LeaderboardDisplayMode::Rejoining:
                if (scorebot::sleep::showSavedScore(millis())) {
                    displays[index]->print(strFormat("%d", scoreFor(state, role)));
                } else {
                    displays[index]->print("PAIR");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Blank:
                displays[index]->clear();
                break;
        }
    }
    if (state.gameStarted && isPlayer(state.whosTurn)) {
        coordinator->setLeaderboardTurnColor(
            getRoleConfig(getNodeIdForRole(state.whosTurn)).color);
    } else {
        coordinator->setLeaderboardTurnColor(0x000000);
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
    DEBUG_PRINTF(
        "State commit: started=%d connected=0x%02x roster=0x%02x turn=%u game=%lu term=%lu version=%lu\n",
        state.gameStarted, static_cast<unsigned>(state.connectedMask),
        static_cast<unsigned>(state.rosterMask), static_cast<unsigned>(state.whosTurn),
        static_cast<unsigned long>(state.gameId), static_cast<unsigned long>(state.term),
        static_cast<unsigned long>(state.version));
    state.refreshDisplays(coordinator);
    replicate(state, coordinator);
}

void rejectAndResync(const GameState& state, Coordinator* coordinator, uint32_t sender) {
    coordinator->ble.sendTo(sender, snapshotJson(state));
}

void sendPlayerActivity(GameState* state, Coordinator* coordinator) {
    if (!state->gameStarted || state->leaderless || !coordinator->ble.hasLeader()) {
        return;
    }
    constexpr uint32_t kActivityThrottleMs = 100;
    const uint32_t now = millis();
    if (state->lastActivitySentMs != 0 &&
        now - state->lastActivitySentMs < kActivityThrottleMs) {
        return;
    }
    const PlayerActivityMessage message(
        coordinator->ble.getMyPeerId(), state->gameId);
    if (coordinator->ble.sendTo(
            getNodeIdForRole(BoardRole::Leader), message.toJson())) {
        state->lastActivitySentMs = now;
    }
}

void sendPlayerOperation(GameState* state, Coordinator* coordinator, bool passesTurn) {
    if (!state->gameStarted || !coordinator->ble.hasLeader() || state->leaderless ||
        state->pendingOperation != 0) {
        setPlayerDisplay(*state, coordinator);
        return;
    }
    const uint32_t operationId = state->nextOperationId();
    PlayerMessage message(state->myScore, passesTurn, coordinator->ble.getMyPeerId(),
                          operationId, state->gameId);
    const String encoded = message.toJson();
    if (coordinator->ble.sendTo(getNodeIdForRole(BoardRole::Leader), encoded)) {
        state->pendingOperation = operationId;
        state->pendingScore = state->myScore;
        // OK always commits the delta. Only a locally active turn is hidden
        // optimistically; the leaderboard independently decides whether the
        // same operation is allowed to advance the turn.
        state->pendingPass = passesTurn && state->whosTurn == coordinator->myRole();
        state->pendingMessage = encoded;
        state->pendingSentMs = millis();
        // The pending payload has its own immutable copy. Clear the submitted
        // entry now so the player can immediately build the next correction;
        // its eventual acknowledgement must not erase that new input.
        state->myScore = 0;
        coordinator->rotaryEncoder.reset();
        setPlayerDisplay(*state, coordinator);
    }
}

void showOtaArmed(Coordinator* coordinator) {
    coordinator->ble.armOta();
    coordinator->display1.print("OTA ");
    coordinator->rotaryEncoder.setColor(0x004040);
}

void resetLeaderboard(GameState* state, Coordinator* coordinator) {
    scorebot::Snapshot rules = toRules(*state);
    if (!scorebot::resetGame(rules)) {
        return;
    }
    fromRules(*state, rules);
    ++state->gameId;
    coordinator->ble.openRoster();
    commit(*state, coordinator);
}

void onButtonPress(GameState* state, const ButtonPressEvent& event, Coordinator* coordinator) {
    const BoardRole role = coordinator->myRole();
    if (event.buttonName == ButtonName::RotaryEncoder) {
        if (coordinator->rotaryEncoder.pressed()) {
            coordinator->noteInteraction();
            if (role != BoardRole::Leader) {
                sendPlayerActivity(state, coordinator);
            }
            if (state->rotaryPressStartedMs.load() == 0) {
                state->rotaryPressStartedMs.store(millis());
            }
            return;
        }

        const uint32_t pressStartedMs = state->rotaryPressStartedMs.exchange(0);
        if (pressStartedMs != 0) {
            const uint32_t heldMs = millis() - pressStartedMs;
            if (heldMs >= kOtaArmHoldMs) {
                showOtaArmed(coordinator);
            } else if (role == BoardRole::Leader) {
                resetLeaderboard(state, coordinator);
            } else {
                // A short rotary press is the existing quick-score action. It
                // remains legal off-turn so an accidental score can be fixed.
                sendPlayerOperation(state, coordinator, false);
            }
            return;
        }

        if (role == BoardRole::Leader) {
            return;
        }
        const int32_t delta = coordinator->rotaryEncoder.delta();
        if (delta != 0 && state->gameStarted && !state->leaderless) {
            coordinator->noteInteraction();
            sendPlayerActivity(state, coordinator);
            state->myScore += delta;
            setPlayerDisplay(*state, coordinator);
        } else if (delta != 0) {
            coordinator->noteInteraction();
            setPlayerDisplay(*state, coordinator);
        }
        return;
    }

    const ButtonGrid::Interrupt interrupt = coordinator->buttonGrid.consumeInterrupt();
    const uint8_t pin = interrupt.pin;
    const uint16_t value = interrupt.captured;
    DEBUG_PRINTF("Button grid: role=%u pin=%u captured=0x%04x pressed=%d\n",
                 static_cast<unsigned>(role), static_cast<unsigned>(pin),
                 static_cast<unsigned>(value), scorebot::buttonCapturedPressed(pin, value));
    if (!scorebot::buttonCapturedPressed(pin, value)) {
        return;
    }
    coordinator->noteInteraction();
    if (role != BoardRole::Leader) {
        sendPlayerActivity(state, coordinator);
    }

    if (role == BoardRole::Leader) {
        scorebot::Snapshot rules = toRules(*state);
        if (pin == ButtonGrid::add && scorebot::start(rules)) {
            fromRules(*state, rules);
            coordinator->ble.freezeRoster(state->rosterMask);
            commit(*state, coordinator);
        } else if (pin == ButtonGrid::okPin) {
            // Some player-style assemblies expose pin 4. The leaderboard
            // hardware does not, so its primary reset control is a short
            // rotary press handled above.
            resetLeaderboard(state, coordinator);
        }
        return;
    }

    if (!state->gameStarted || state->leaderless) {
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
    if (!isPlayer(senderConfig.role) || message.fromNodeId != sender ||
        message.gameId != state->gameId || !state->gameStarted) {
        rejectAndResync(*state, coordinator, sender);
        return;
    }
    coordinator->noteInteraction();
    scorebot::Snapshot rules = toRules(*state);
    const uint32_t previousVersion = rules.version;
    const scorebot::ApplyResult result = scorebot::apply(
        rules, {toRulePlayer(senderConfig.role), message.score, message.turnPassed, message.operationId});
    if (result == scorebot::ApplyResult::Accepted ||
        result == scorebot::ApplyResult::AcceptedWithoutTurnChange ||
        rules.version != previousVersion) {
        // Valid rejected operations are recorded as consumed so a player can
        // distinguish rejection from packet loss and stop retrying them.
        fromRules(*state, rules);
        commit(*state, coordinator);
    } else {
        rejectAndResync(*state, coordinator, sender);
    }
}

void onPlayerActivityMessage(
    GameState* state, const Event& event, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader || !state->gameStarted) {
        return;
    }
    const PlayerActivityMessage message =
        PlayerActivityMessage::fromJson(event.messageReceived.message);
    const uint32_t sender = event.messageReceived.peerId;
    const BoardRole senderRole = getRoleConfig(sender).role;
    if (!isPlayer(senderRole) || message.fromNodeId != sender ||
        message.gameId != state->gameId) {
        return;
    }
    const scorebot::Player player = toRulePlayer(senderRole);
    const scorebot::Snapshot rules = toRules(*state);
    if (!scorebot::isConnected(rules, player) ||
        (rules.rosterMask & (1u << scorebot::playerIndex(player))) == 0) {
        return;
    }
    DEBUG_PRINTF("Player activity: from=%08lx game=%lu\n",
                 static_cast<unsigned long>(sender),
                 static_cast<unsigned long>(message.gameId));
    coordinator->noteInteraction();
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
      rosterMask(0),
      gameStarted(false),
      gameId(0),
      term(0),
      version(0),
      leaderId(0),
      leaderless(true),
      lastOperation{},
      localOperation(0),
      lastReplicationMs(0),
      lastRejoinDisplayMs(0),
      pendingOperation(0),
      pendingScore(0),
      pendingPass(false),
      pendingMessage(),
      pendingSentMs(0),
      lastActivitySentMs(0),
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
    const uint32_t pressStartedMs = rotaryPressStartedMs.load();
    if (pressStartedMs != 0 &&
        static_cast<uint32_t>(now - pressStartedMs) >= kOtaArmHoldMs) {
        const bool stillPressed = coordinator->rotaryEncoder.pressed();
        rotaryPressStartedMs.store(0);
        if (scorebot::otaArmHoldReached(
                stillPressed, now, pressStartedMs, kOtaArmHoldMs)) {
            // Arm as soon as the threshold is reached. This gives immediate
            // feedback and does not depend on receiving a second interrupt
            // when the button is released.
            showOtaArmed(coordinator);
        }
    }
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
    if (gameStarted && (connectedMask & rosterMask) != rosterMask &&
        now - lastRejoinDisplayMs >= scorebot::sleep::kRejoinAlternateMs) {
        setLeaderboardDisplays(*this, coordinator);
        lastRejoinDisplayMs = now;
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
            } else if (PlayerActivityMessage::isPlayerActivityMessage(json)) {
                onPlayerActivityMessage(this, event, coordinator);
            } else {
                const bool wasGameStarted = gameStarted;
                const uint32_t previousGameId = gameId;
                const uint32_t previousTerm = term;
                const SnapshotDecodeResult decoded = decodeSnapshot(*this, json);
                if (decoded == SnapshotDecodeResult::Invalid ||
                    decoded == SnapshotDecodeResult::Older) {
                    break;
                }
                const BoardRole myRole = coordinator->myRole();
                if (isPlayer(myRole)) {
                    coordinator->ble.confirmLeader(event.messageReceived.connectionHandle);
                    const bool acknowledged = scorebot::operationAcknowledged(
                        pendingOperation, operationFor(*this, myRole));
                    const scorebot::PendingDisposition pendingDisposition =
                        scorebot::pendingDisposition(
                            pendingOperation != 0, acknowledged,
                            gameId != previousGameId, term != previousTerm);
                    if (pendingDisposition == scorebot::PendingDisposition::Restore) {
                        // The authoritative epoch changed before this request
                        // was committed. Restore its delta for explicit retry.
                        myScore += pendingScore;
                    }
                    localOperation = scorebot::reconcileOperationId(
                        localOperation, operationFor(*this, myRole));
                    if (pendingDisposition != scorebot::PendingDisposition::Keep) {
                        pendingOperation = 0;
                        pendingScore = 0;
                        pendingPass = false;
                        pendingMessage = "";
                        pendingSentMs = 0;
                    }
                    if (gameId != previousGameId || (wasGameStarted && !gameStarted)) {
                        // A leaderboard reset starts a clean lobby. Discard
                        // transient input and any request from the old game.
                        pendingOperation = 0;
                        pendingScore = 0;
                        pendingPass = false;
                        pendingMessage = "";
                        pendingSentMs = 0;
                        myScore = 0;
                        coordinator->rotaryEncoder.reset();
                    }
                }
                if (decoded == SnapshotDecodeResult::Equal) {
                    // A rebooted player commonly receives only equal revision
                    // heartbeats. They prove the fixed leaderboard is live.
                    leaderless = false;
                    refreshDisplays(coordinator);
                    break;
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
