#include <Coordinator.hpp>
#include <ButtonInputRules.hpp>
#include <ErrorHandler.hpp>
#include <GameState.hpp>
#include <GameRules.hpp>
#include <LeaderboardControlRules.hpp>
#include <LeaderboardUiRules.hpp>
#include <LightColorRules.hpp>
#include <MessageAuthorityRules.hpp>
#include <MaintenanceRules.hpp>
#include <Messages.hpp>
#include <OtaTransferRules.hpp>
#include <PlayerUiRules.hpp>
#include <PrinterRules.hpp>
#include <Protocol.hpp>
#include <ReplicationRules.hpp>
#include <SleepRules.hpp>
#include <VisualFeedbackRules.hpp>
#include <utils.hpp>

#include <Preferences.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace {
constexpr char kStateNamespace[] = "scorebot";
constexpr char kStateKey[] = "state";
constexpr uint32_t kReplicationPeriodMs = 1500;
constexpr uint32_t kOperationRetryMs = 1000;
constexpr uint32_t kLobbyModeSettleMs = 250;

static_assert(scorebot::kPlayerAddButtonMask == (1u << ButtonGrid::add));
static_assert(
    scorebot::kPlayerNegativeOneButtonMask == (1u << ButtonGrid::negone));
static_assert(
    scorebot::kPlayerPlusFiveButtonMask == (1u << ButtonGrid::plusfive));
static_assert(
    scorebot::kPlayerPlusOneButtonMask == (1u << ButtonGrid::plusone));
static_assert(scorebot::kPlayerOkButtonMask == (1u << ButtonGrid::okPin));
static_assert(
    scorebot::kLeaderboardAddButtonMask == (1u << ButtonGrid::negone));
static_assert(
    scorebot::kLeaderboardNegativeOneButtonMask ==
    (1u << ButtonGrid::plusone));
static_assert(
    scorebot::kLeaderboardPlusOneButtonMask == (1u << ButtonGrid::add));
static_assert(
    scorebot::kLeaderboardOkButtonMask == (1u << ButtonGrid::plusfive));

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
    rules.localControlMask = state.localControlMask;
    rules.lobbyEnabledMask = state.lobbyEnabledMask;
    rules.lobbyExplicitMask = state.lobbyExplicitMask;
    rules.sleepingMask = state.sleepingMask;
    rules.rosterMask = state.rosterMask;
    return rules;
}

void fromRules(GameState& state, const scorebot::Snapshot& rules) {
    state.gameStarted = rules.started;
    const BoardRole nextTurn = fromRulePlayer(rules.turn);
    if (state.whosTurn != nextTurn) {
        state.turnStatusStartedMs = millis();
        state.lastTurnStatusFrame = 0;
    }
    state.whosTurn = nextTurn;
    state.version = rules.version;
    state.connectedMask = rules.connectedMask;
    state.localControlMask = rules.localControlMask;
    state.lobbyEnabledMask = rules.lobbyEnabledMask;
    state.lobbyExplicitMask = rules.lobbyExplicitMask;
    state.sleepingMask = rules.sleepingMask;
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
    document["local"] = state.localControlMask;
    document["enabled"] = state.lobbyEnabledMask;
    document["explicit"] = state.lobbyExplicitMask;
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
        !document["leader"].is<uint32_t>() || !document["local"].is<uint32_t>() ||
        !document["scores"].is<JsonArray>() ||
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
    const uint32_t localControlMask = document["local"].as<uint32_t>();
    const uint32_t lobbyEnabledMask = document["enabled"].is<uint32_t>()
                                          ? document["enabled"].as<uint32_t>()
                                          : 0x0f;
    const uint32_t lobbyExplicitMask = document["explicit"].is<uint32_t>()
                                           ? document["explicit"].as<uint32_t>()
                                           : lobbyEnabledMask;
    const bool rosterPresent = document["roster"].is<uint32_t>();
    const uint32_t rosterMask = rosterPresent ? document["roster"].as<uint32_t>() : 0;
    // Older snapshots did not distinguish a frozen roster from live links.
    // Resume those in the lobby instead of accidentally locking out a board.
    const bool gameStarted = (document["started"] | false) && rosterPresent;
    const BoardRole turn = static_cast<BoardRole>(document["turn"] | 0);
    if ((connectedMask & ~0x0fu) != 0 || (localControlMask & ~0x0fu) != 0 ||
        (lobbyEnabledMask & ~0x0fu) != 0 ||
        (lobbyExplicitMask & ~0x0fu) != 0 ||
        (localControlMask & ~lobbyEnabledMask) != 0 ||
        (rosterMask & ~0x0fu) != 0 ||
        ((localControlMask & ~rosterMask) != 0 && gameStarted) ||
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
    const BoardRole nextTurn = gameStarted ? turn : BoardRole::Unknown;
    if (state.whosTurn != nextTurn) {
        state.turnStatusStartedMs = millis();
        state.lastTurnStatusFrame = 0;
    }
    state.whosTurn = nextTurn;
    state.connectedMask = static_cast<uint8_t>(connectedMask);
    state.localControlMask = static_cast<uint8_t>(localControlMask);
    state.lobbyEnabledMask = static_cast<uint8_t>(lobbyEnabledMask);
    state.lobbyExplicitMask = static_cast<uint8_t>(lobbyExplicitMask);
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
            if (scorebot::turnStatusShowsScore(
                    millis() - state.turnStatusStartedMs)) {
                coordinator->display1.print(strFormat(
                    "%d", scoreFor(state, coordinator->myRole())));
            } else {
                coordinator->display1.print("GO  ");
            }
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
    const size_t selectedIndex = roleIndex(state.selectedLeaderboardRole);
    bool selectedLocalPrompt = false;
    if (!state.gameStarted && selectedIndex < std::size(kPlayers)) {
        const uint8_t selectedBit = static_cast<uint8_t>(1u << selectedIndex);
        selectedLocalPrompt =
            (state.localControlMask & selectedBit) != 0;
    }
    coordinator->setLeaderboardSelectedDisplay(
        selectedIndex < std::size(kPlayers) ? static_cast<int8_t>(selectedIndex) : -1,
        !state.gameStarted, selectedLocalPrompt);
    // Publish the authoritative turn light before writing display content. A
    // newly rendered GO must never coexist with the previous turn's color.
    if (state.gameStarted && isPlayer(state.whosTurn)) {
        coordinator->setLeaderboardTurnColor(
            getRoleConfig(getNodeIdForRole(state.whosTurn)).color);
    } else {
        coordinator->setLeaderboardTurnColor(0x000000);
    }
    if (!coordinator->normalUiWritesAllowed()) {
        return;
    }
    const bool showSavedScore = scorebot::sleep::showSavedScore(millis());
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        const bool connected = (state.connectedMask & (1u << index)) != 0;
        const bool locallyControlled = (state.localControlMask & (1u << index)) != 0;
        const bool lobbyEnabled = (state.lobbyEnabledMask & (1u << index)) != 0;
        const bool sleeping = (state.sleepingMask & (1u << index)) != 0;
        const bool inRoster = (state.rosterMask & (1u << index)) != 0;
        if (state.gameStarted && inRoster && state.leaderboardDeltas[index] != 0) {
            displays[index]->print(strFormat("%d", state.leaderboardDeltas[index]));
            continue;
        }
        switch (scorebot::leaderboardDisplayMode(
            state.gameStarted, connected, inRoster, sleeping,
            locallyControlled, lobbyEnabled,
            state.whosTurn == role)) {
            case scorebot::LeaderboardDisplayMode::Score:
                displays[index]->print(strFormat("%d", scoreFor(state, role)));
                break;
            case scorebot::LeaderboardDisplayMode::LobbyName:
                displays[index]->print(getRoleConfig(getNodeIdForRole(role)).name);
                break;
            case scorebot::LeaderboardDisplayMode::Pairing:
                displays[index]->print("PAIR");
                break;
            case scorebot::LeaderboardDisplayMode::LocalControl:
                if (index == selectedIndex) {
                    const auto prompt = scorebot::leaderboardLocalPromptFrame(0);
                    displays[index]->print(prompt.data());
                } else {
                    displays[index]->print("LOCL");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Off:
                displays[index]->print("OFF ");
                break;
            case scorebot::LeaderboardDisplayMode::Turn:
                if (scorebot::turnStatusShowsScore(
                        millis() - state.turnStatusStartedMs)) {
                    displays[index]->print(strFormat("%d", scoreFor(state, role)));
                } else {
                    displays[index]->print("GO  ");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Rejoining:
                if (showSavedScore) {
                    displays[index]->print(strFormat("%d", scoreFor(state, role)));
                } else {
                    displays[index]->print("PAIR");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Sleeping:
                if (state.gameStarted && inRoster && showSavedScore) {
                    displays[index]->print(strFormat("%d", scoreFor(state, role)));
                } else {
                    displays[index]->print("ZZZZ");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Blank:
                displays[index]->clear();
                break;
        }
    }
}

void replicate(const GameState& state, Coordinator* coordinator) {
    coordinator->ble.sendBroadcast(snapshotJson(state));
}

void selectAuthoritativeTurn(GameState& state, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    if (!state.gameStarted) {
        if (!isPlayer(state.selectedLeaderboardRole)) {
            state.selectedLeaderboardRole = BoardRole::Player_Red;
        }
        return;
    }
    const scorebot::Snapshot rules = toRules(state);
    state.selectedLeaderboardRole = fromRulePlayer(scorebot::selectedTurnTarget(
        rules.turn, scorebot::leaderboardControlMask(rules)));
}

void cycleLeaderboardSelection(GameState& state, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const uint8_t selectableMask = state.gameStarted
                                       ? scorebot::leaderboardControlMask(toRules(state))
                                       : 0x0f;
    state.selectedLeaderboardRole = fromRulePlayer(scorebot::nextLeaderboardTarget(
        toRulePlayer(state.selectedLeaderboardRole), selectableMask));
    state.refreshDisplays(coordinator);
}

void commit(GameState& state, Coordinator* coordinator) {
    state.leaderId = coordinator->ble.getMyPeerId();
    state.leaderless = false;
    selectAuthoritativeTurn(state, coordinator);
    // The leaderboard is the authority, so publish and replicate its accepted
    // state immediately. Give an immutable snapshot to the low-priority
    // persistence worker; flash I/O must never hold the gameplay/UI lock.
    state.refreshDisplays(coordinator);
    state.lobbyModeDirty = false;
    const String encoded = snapshotJson(state);
    if (coordinator->enqueueStatePersistence(encoded) == 0) {
        FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "queue authoritative state");
        return;
    }
    DEBUG_PRINTF(
        "State commit: started=%d connected=0x%02x roster=0x%02x turn=%u game=%lu term=%lu version=%lu\n",
        state.gameStarted, static_cast<unsigned>(state.connectedMask),
        static_cast<unsigned>(state.rosterMask), static_cast<unsigned>(state.whosTurn),
        static_cast<unsigned long>(state.gameId), static_cast<unsigned long>(state.term),
        static_cast<unsigned long>(state.version));
    coordinator->ble.sendBroadcast(encoded);
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

void addDelta(int32_t& pending, int32_t delta) {
    const int64_t next = static_cast<int64_t>(pending) + delta;
    pending = static_cast<int32_t>(std::clamp<int64_t>(
        next, std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max()));
}

void sendLeaderboardOperation(
    GameState* state, Coordinator* coordinator, bool passesTurn) {
    const size_t index = roleIndex(state->selectedLeaderboardRole);
    if (!state->gameStarted || index >= std::size(kPlayers)) {
        state->refreshDisplays(coordinator);
        return;
    }
    if (!passesTurn && state->leaderboardDeltas[index] == 0) {
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    const scorebot::ApplyResult result = scorebot::applyLocal(
        rules,
        {toRulePlayer(state->selectedLeaderboardRole),
         state->leaderboardDeltas[index], passesTurn});
    if (result == scorebot::ApplyResult::Accepted ||
        result == scorebot::ApplyResult::AcceptedWithoutTurnChange) {
        state->leaderboardDeltas[index] = 0;
        coordinator->rotaryEncoder.reset();
        fromRules(*state, rules);
        commit(*state, coordinator);
    } else {
        state->refreshDisplays(coordinator);
    }
}

void clearTransientInput(GameState& state) {
    state.myScore = 0;
    state.leaderboardDeltas.fill(0);
    state.pendingOperation = 0;
    state.pendingScore = 0;
    state.pendingPass = false;
    state.pendingMessage = "";
    state.pendingSentMs = 0;
    state.localOperation = 0;
}

void clearCurrentBoardState(GameState* state, Coordinator* coordinator) {
    Preferences preferences;
    if (!coordinator->waitForStatePersistence(5000)) {
        FATAL_ERROR(
            ErrorCode::STATE_PERSIST_FAILED,
            "drain state persistence before reset");
    }
    const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
    // Establish the exclusive dispatcher/BLE gate while the state mutex is
    // still held, before either the authoritative snapshot or local NVS can
    // change underneath a newly arriving event.
    coordinator->requestRestart(leaderboard);
    if (leaderboard) {
        const uint32_t nextTerm = state->term + 1;
        const uint32_t nextGame = state->gameId + 1;
        state->scores.fill(0);
        state->lastOperation.fill(0);
        state->connectedMask = 0;
        state->localControlMask = 0;
        state->lobbyEnabledMask = 0;
        state->lobbyExplicitMask = 0;
        state->sleepingMask = 0;
        state->rosterMask = 0;
        state->whosTurn = BoardRole::Unknown;
        state->gameStarted = false;
        state->gameId = nextGame;
        state->term = nextTerm;
        state->version = 0;
        state->leaderId = coordinator->ble.getMyPeerId();
        state->leaderless = false;
        state->selectedLeaderboardRole = BoardRole::Player_Red;
        clearTransientInput(*state);
        coordinator->ble.openRoster();
        if (!state->persist()) {
            FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "leader local-state reset");
        }
        const String resetSnapshot = LeaderResetMessage(
            state->gameId, state->term, state->leaderId).toJson();
        constexpr uint32_t kResetTransmissionRetryMs = 1000;
        const uint32_t transmission = coordinator->ble.sendBroadcastTracked(
            resetSnapshot, kResetTransmissionRetryMs);
        if (transmission == 0) {
            FATAL_ERROR(
                ErrorCode::BLE_SEND_FAILED,
                "queue authoritative reset snapshot");
        }
        coordinator->setRestartTransmission(transmission);
        // Reboot is now owned by Coordinator::loop, the same task that drains
        // BLE. It waits for this request to be handed to NimBLE and keeps the
        // radio alive for several connection intervals before restarting.
    } else {
        if (!preferences.begin(kStateNamespace, false) || !preferences.clear()) {
            FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "player local-state reset");
        }
        preferences.end();
    }
    coordinator->finishRestartPreparation();
}

void startMaintenance(
    GameState* state, Coordinator* coordinator,
    scorebot::MaintenanceAction action, uint16_t pressedMask) {
    state->maintenanceAction = action;
    state->maintenanceStartedMs = millis();
    const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
    state->suppressedButtonMask |=
        scorebot::maintenanceChordMask(action, leaderboard);
    state->heldButtonMask = pressedMask;
    coordinator->noteInteraction();
    coordinator->startMaintenanceUi(action, state->maintenanceStartedMs);
}

void cancelMaintenance(GameState* state, Coordinator* coordinator, uint16_t pressedMask) {
    const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
    const uint16_t chordMask = scorebot::maintenanceChordMask(
        state->maintenanceAction, leaderboard);
    state->suppressedButtonMask = static_cast<uint16_t>(pressedMask & chordMask);
    state->heldButtonMask = pressedMask;
    state->maintenanceAction = scorebot::MaintenanceAction::None;
    state->maintenanceStartedMs = 0;
    coordinator->cancelMaintenanceUi();
}

void applySingleButton(GameState* state, uint8_t pin, Coordinator* coordinator) {
    const BoardRole role = coordinator->myRole();
    const scorebot::ButtonAction action = scorebot::buttonActionForPin(
        role == BoardRole::Leader, pin);
    if (action == scorebot::ButtonAction::None) {
        return;
    }
    coordinator->noteInteraction();
    if (role != BoardRole::Leader) {
        sendPlayerActivity(state, coordinator);
    }

    if (role == BoardRole::Leader) {
        if (!state->gameStarted) {
            if (action == scorebot::ButtonAction::Ok) {
                scorebot::Snapshot rules = toRules(*state);
                if (scorebot::start(rules)) {
                    fromRules(*state, rules);
                    state->lobbyModeDirty = false;
                    coordinator->ble.freezeRoster(state->rosterMask);
                    commit(*state, coordinator);
                }
            }
            return;
        }

        const size_t index = roleIndex(state->selectedLeaderboardRole);
        if (index >= std::size(kPlayers) ||
            !scorebot::isLocallyControllable(
                toRules(*state), toRulePlayer(state->selectedLeaderboardRole))) {
            state->refreshDisplays(coordinator);
            return;
        }
        if (action == scorebot::ButtonAction::Add) {
            sendLeaderboardOperation(state, coordinator, false);
            return;
        }
        if (action == scorebot::ButtonAction::NegativeOne) {
            addDelta(state->leaderboardDeltas[index], -1);
        } else if (action == scorebot::ButtonAction::PlusOne) {
            addDelta(state->leaderboardDeltas[index], 1);
        } else if (action == scorebot::ButtonAction::PlusFive) {
            addDelta(state->leaderboardDeltas[index], 5);
        } else if (action == scorebot::ButtonAction::Ok) {
            sendLeaderboardOperation(state, coordinator, true);
            return;
        }
        state->refreshDisplays(coordinator);
        return;
    }

    if (!state->gameStarted || state->leaderless) {
        setPlayerDisplay(*state, coordinator);
        return;
    }
    if (action == scorebot::ButtonAction::Add) {
        if (state->myScore != 0) {
            sendPlayerOperation(state, coordinator, false);
        }
    } else if (action == scorebot::ButtonAction::NegativeOne) {
        addDelta(state->myScore, -1);
    } else if (action == scorebot::ButtonAction::PlusOne) {
        addDelta(state->myScore, 1);
    } else if (action == scorebot::ButtonAction::PlusFive) {
        addDelta(state->myScore, 5);
    } else if (action == scorebot::ButtonAction::Ok) {
        sendPlayerOperation(state, coordinator, true);
        return;
    }
    setPlayerDisplay(*state, coordinator);
}

void onButtonGridEvent(GameState* state, Coordinator* coordinator) {
    const ButtonGrid::Interrupt interrupt = coordinator->buttonGrid.consumeInterrupt();
    const uint16_t current = interrupt.pressed;
    const uint16_t previous = state->heldButtonMask;
    const uint16_t previouslySuppressed = state->suppressedButtonMask;
    const bool capturedPress =
        scorebot::buttonCapturedPressed(interrupt.pin, interrupt.captured);
    DEBUG_PRINTF(
        "Button grid: role=%u pin=%u captured=0x%04x current=0x%04x pressed=%d\n",
        static_cast<unsigned>(coordinator->myRole()),
        static_cast<unsigned>(interrupt.pin),
        static_cast<unsigned>(interrupt.captured),
        static_cast<unsigned>(current), capturedPress);

#if defined(SCOREBOT_BUTTON_DIAGNOSTIC)
    // A passive diagnostic must report both edges without letting ADD, OK, or
    // a maintenance chord mutate the persisted game while wiring is probed.
    state->heldButtonMask = current;
    state->suppressedButtonMask = 0;
    return;
#endif

    if (coordinator->printerUiActive()) {
        // Keep the level model synchronized while the printer owns the local
        // UI. Release edges belonging to the completed PRINT chord are drained
        // without dismissing DONE/error; only a later, ordinary input may
        // dismiss a terminal result.
        state->heldButtonMask = current;
        if (previouslySuppressed != 0) {
            state->suppressedButtonMask = current;
            return;
        }
        state->suppressedButtonMask = current;
        coordinator->noteInteraction();
        (void)coordinator->consumePrinterUiInput();
        return;
    }

    if (state->maintenanceAction != scorebot::MaintenanceAction::None) {
        const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
        const scorebot::MaintenanceAction next =
            scorebot::maintenanceActionTransition(
                state->maintenanceAction, current, leaderboard);
        if (next != state->maintenanceAction) {
            // The four-button leaderboard gesture can be assembled through
            // either two-button maintenance chord. Start its safety interval
            // when the fourth button actually arrives.
            startMaintenance(state, coordinator, next, current);
        } else if (!scorebot::maintenanceChordHeld(
                       state->maintenanceAction, current, leaderboard)) {
            cancelMaintenance(state, coordinator, current);
        } else {
            state->heldButtonMask = current;
        }
        return;
    }

    if (previouslySuppressed != 0) {
        // A cancelled/completed chord owns all of its release edges. In
        // particular, releasing an all-button PRINT chord passes through the
        // RESET and OTA masks; neither partial mask may start a fresh hold.
        // Extend suppression to any button pressed before the gesture is fully
        // released so no deferred single-button action can leak through.
        state->heldButtonMask = current;
        state->suppressedButtonMask = current;
        return;
    }

    const scorebot::MaintenanceAction chord =
        scorebot::maintenanceActionTransition(
            scorebot::MaintenanceAction::None, current,
            coordinator->myRole() == BoardRole::Leader);
    if (chord != scorebot::MaintenanceAction::None) {
        startMaintenance(state, coordinator, chord, current);
        return;
    }

    const uint16_t released = static_cast<uint16_t>(previous & ~current);
    const uint16_t interruptBit = interrupt.pin < 16
                                      ? static_cast<uint16_t>(1u << interrupt.pin)
                                      : 0;
    const uint16_t fastTap = capturedPress && (current & interruptBit) == 0 &&
                                     (previous & interruptBit) == 0
                                 ? interruptBit
                                 : 0;
    const uint16_t completed = static_cast<uint16_t>(released | fastTap);
    state->heldButtonMask = current;
    state->suppressedButtonMask = static_cast<uint16_t>(
        previouslySuppressed & current);

    // Resolve every single-button action on release. That gives either chord
    // time to capture its second key without leaking a score or turn action.
    constexpr uint8_t deferredPins[] = {
        ButtonGrid::add,
        ButtonGrid::negone,
        ButtonGrid::plusfive,
        ButtonGrid::plusone,
        ButtonGrid::okPin,
    };
    for (const uint8_t pin : deferredPins) {
        const uint16_t bit = static_cast<uint16_t>(1u << pin);
        if ((completed & bit) != 0 && (previouslySuppressed & bit) == 0) {
            applySingleButton(state, pin, coordinator);
        }
    }
}

void onButtonPress(GameState* state, const ButtonPressEvent& event, Coordinator* coordinator) {
    const BoardRole role = coordinator->myRole();
    if (coordinator->printerUiActive()) {
        if (event.buttonName == ButtonName::GPIOButtons) {
            onButtonGridEvent(state, coordinator);
        } else {
            (void)coordinator->rotaryEncoder.pressed();
            (void)coordinator->rotaryEncoder.delta();
            state->rotaryPressStartedMs.store(0);
            coordinator->noteInteraction();
            (void)coordinator->consumePrinterUiInput();
        }
        return;
    }
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

        if (state->rotaryPressStartedMs.exchange(0) != 0) {
            if (role == BoardRole::Leader) {
                cycleLeaderboardSelection(*state, coordinator);
            } else {
                sendPlayerOperation(state, coordinator, false);
            }
            return;
        }

        const int32_t delta = coordinator->rotaryEncoder.delta();
        if (delta == 0) {
            return;
        }
        coordinator->noteInteraction();
        if (role == BoardRole::Leader) {
            if (!state->gameStarted) {
                const scorebot::Player player =
                    toRulePlayer(state->selectedLeaderboardRole);
                scorebot::Snapshot rules = toRules(*state);
                const scorebot::LobbyMode next = scorebot::rotatedLobbyMode(
                    scorebot::lobbyModeFor(rules, player), delta);
                if (scorebot::setLobbyMode(rules, player, next)) {
                    fromRules(*state, rules);
                    state->lobbyModeDirty = true;
                    state->lobbyModeChangedMs = millis();
                    state->refreshDisplays(coordinator);
                } else {
                    state->refreshDisplays(coordinator);
                }
                return;
            }
            const size_t index = roleIndex(state->selectedLeaderboardRole);
            if (index < std::size(kPlayers) && scorebot::isLocallyControllable(
                    toRules(*state), toRulePlayer(state->selectedLeaderboardRole))) {
                addDelta(state->leaderboardDeltas[index], delta);
                state->refreshDisplays(coordinator);
            }
            return;
        }
        if (state->gameStarted && !state->leaderless) {
            sendPlayerActivity(state, coordinator);
            addDelta(state->myScore, delta);
        }
        setPlayerDisplay(*state, coordinator);
        return;
    }

    onButtonGridEvent(state, coordinator);
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
    const BoardRoleConfig& senderConfig = getRoleConfig(sender);
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

void onPlayerSleepMessage(
    GameState* state, const Event& event, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const PlayerSleepMessage message =
        PlayerSleepMessage::fromJson(event.messageReceived.message);
    const uint32_t sender = event.messageReceived.peerId;
    const BoardRole senderRole = getRoleConfig(sender).role;
    const scorebot::Player player = toRulePlayer(senderRole);
    if (!scorebot::isPlayer(player) || message.fromNodeId != sender ||
        !scorebot::isConnected(toRules(*state), player)) {
        return;
    }
    state->sleepingMask |= static_cast<uint8_t>(
        1u << scorebot::playerIndex(player));
    DEBUG_PRINTF("Player sleeping: from=%08lx\n",
                 static_cast<unsigned long>(sender));
    state->refreshDisplays(coordinator);
}

void onLeaderResetMessage(
    GameState* state, const Event& event, Coordinator* coordinator) {
    const uint32_t sender = event.messageReceived.peerId;
    if (!scorebot::mayAcceptSnapshot(
            coordinator->myRole(), getRoleConfig(sender).role)) {
        return;
    }
    const LeaderResetMessage message =
        LeaderResetMessage::fromJson(event.messageReceived.message);
    if (message.leaderId != sender ||
        sender != getNodeIdForRole(BoardRole::Leader)) {
        return;
    }
    const scorebot::RevisionOrder order = scorebot::compareRevision(
        {message.term, 0}, {state->term, state->version});
    if (order == scorebot::RevisionOrder::Older) {
        return;
    }
    coordinator->ble.confirmLeader(event.messageReceived.connectionHandle);
    state->leaderless = false;
    if (order == scorebot::RevisionOrder::Equal) {
        state->refreshDisplays(coordinator);
        return;
    }

    state->scores.fill(0);
    state->lastOperation.fill(0);
    state->connectedMask = 0;
    state->localControlMask = 0;
    state->lobbyEnabledMask = 0;
    state->lobbyExplicitMask = 0;
    state->sleepingMask = 0;
    state->rosterMask = 0;
    state->whosTurn = BoardRole::Unknown;
    state->gameStarted = false;
    state->gameId = message.gameId;
    state->term = message.term;
    state->version = 0;
    state->leaderId = message.leaderId;
    state->selectedLeaderboardRole = BoardRole::Player_Red;
    state->turnStatusStartedMs = millis();
    state->lastTurnStatusFrame = 0;
    state->lobbyModeDirty = false;
    clearTransientInput(*state);
    coordinator->rotaryEncoder.reset();
    state->refreshDisplays(coordinator);
    const String encoded = snapshotJson(*state);
    if (coordinator->enqueueStatePersistence(encoded) == 0) {
        FATAL_ERROR(
            ErrorCode::STATE_PERSIST_FAILED,
            "queue compact authoritative reset");
    }
}

void onNewPeer(GameState* state, const Event& event, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const scorebot::Player player =
        toRulePlayer(getRoleConfig(event.newPeer.peerId).role);
    if (scorebot::isPlayer(player)) {
        const size_t index = scorebot::playerIndex(player);
        state->sleepingMask &= static_cast<uint8_t>(
            ~(1u << index));
    }
    scorebot::Snapshot rules = toRules(*state);
    if (scorebot::connect(rules, player)) {
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
      turnStatusStartedMs(0),
      lastTurnStatusFrame(0xffffffff),
      scores{},
      connectedMask(0),
      localControlMask(0),
      lobbyEnabledMask(0),
      lobbyExplicitMask(0),
      sleepingMask(0),
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
      leaderboardDeltas{},
      selectedLeaderboardRole(BoardRole::Player_Red),
      lobbyModeDirty(false),
      lobbyModeChangedMs(0),
      heldButtonMask(0),
      suppressedButtonMask(0),
      maintenanceAction(scorebot::MaintenanceAction::None),
      maintenanceStartedMs(0),
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
    const String encoded = snapshotJson(*this);
    return persistEncoded(encoded.c_str());
}

bool GameState::persistEncoded(const char* encoded) {
    if (encoded == nullptr) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(kStateNamespace, false)) {
        return false;
    }
    const size_t length = strlen(encoded);
    const bool stored = preferences.putString(kStateKey, encoded) == length;
    preferences.end();
    return stored;
}

uint32_t GameState::nextOperationId() {
    return ++localOperation;
}

void GameState::refreshDisplays(Coordinator* coordinator) const {
    setLeaderboardDisplays(*this, coordinator);
    setPlayerDisplay(*this, coordinator);
}

void GameState::syncLeaderboardSelection(Coordinator* coordinator) {
    selectAuthoritativeTurn(*this, coordinator);
}

bool GameState::hasPendingLocalScore() const {
    if (myScore != 0) {
        return true;
    }
    return std::any_of(
        leaderboardDeltas.begin(), leaderboardDeltas.end(),
        [](int32_t delta) { return delta != 0; });
}

void GameState::heartbeat(Coordinator* coordinator) {
    const uint32_t now = millis();
    if (maintenanceAction != scorebot::MaintenanceAction::None) {
        const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
        const uint16_t pressed = coordinator->buttonGrid.pressedMask();
        const scorebot::MaintenanceAction next =
            scorebot::maintenanceActionTransition(
                maintenanceAction, pressed, leaderboard);
        if (next != maintenanceAction) {
            // The hardware level can show the fourth button before its queued
            // interrupt reaches the dispatcher. Promote here too, so a RESET
            // or OTA hold can never complete underneath a physical PRINT
            // chord. PRINT receives a fresh five-second safety interval.
            startMaintenance(this, coordinator, next, pressed);
            return;
        }
        const uint16_t required = scorebot::maintenanceChordMask(
            maintenanceAction, leaderboard);
        if (!scorebot::maintenanceChordHeld(
                maintenanceAction, pressed, leaderboard)) {
            cancelMaintenance(this, coordinator, pressed);
            return;
        }
        heldButtonMask = pressed;
        if (scorebot::maintenanceHoldComplete(now - maintenanceStartedMs)) {
            const scorebot::MaintenanceAction completed = maintenanceAction;
            maintenanceAction = scorebot::MaintenanceAction::None;
            maintenanceStartedMs = 0;
            suppressedButtonMask |= required;
            if (completed == scorebot::MaintenanceAction::Reset) {
                clearCurrentBoardState(this, coordinator);
                return;
            }
            if (completed == scorebot::MaintenanceAction::Ota) {
                coordinator->armOta();
                return;
            }
            scorebot::PrinterSnapshot snapshot{};
            snapshot.leaderId = leaderId;
            snapshot.gameId = gameId;
            snapshot.term = term;
            snapshot.version = version;
            snapshot.started = gameStarted;
            snapshot.scores = scores;
            snapshot.turn = toRulePlayer(whosTurn);
            (void)coordinator->startPrint(snapshot);
        }
        return;
    }
    const BoardRole localRole = coordinator->myRole();
    const bool turnStatusVisible =
        gameStarted && isPlayer(whosTurn) &&
        (localRole == BoardRole::Leader || localRole == whosTurn);
    if (turnStatusVisible) {
        const uint32_t frame = scorebot::turnStatusFrame(
            now - turnStatusStartedMs);
        if (frame != lastTurnStatusFrame) {
            lastTurnStatusFrame = frame;
            refreshDisplays(coordinator);
        }
    } else {
        lastTurnStatusFrame = 0xffffffff;
    }

    if (localRole != BoardRole::Leader) {
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
    if (!gameStarted && lobbyModeDirty &&
        now - lobbyModeChangedMs >= kLobbyModeSettleMs) {
        commit(*this, coordinator);
    }
    const uint8_t directlyAvailable = static_cast<uint8_t>(
        connectedMask | localControlMask);
    if (gameStarted && (directlyAvailable & rosterMask) != rosterMask &&
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
            } else if (PlayerSleepMessage::isPlayerSleepMessage(json)) {
                onPlayerSleepMessage(this, event, coordinator);
            } else if (LeaderResetMessage::isLeaderResetMessage(json)) {
                onLeaderResetMessage(this, event, coordinator);
            } else {
                // State replication is one-way. Never let a player uplink
                // replace authoritative leaderboard state, regardless of the
                // identity embedded in its JSON payload.
                if (!scorebot::mayAcceptSnapshot(
                        coordinator->myRole(),
                        getRoleConfig(event.messageReceived.peerId).role)) {
                    break;
                }
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
                refreshDisplays(coordinator);
                const String encoded = snapshotJson(*this);
                if (coordinator->enqueueStatePersistence(encoded) == 0) {
                    FATAL_ERROR(
                        ErrorCode::STATE_PERSIST_FAILED,
                        "queue replicated player state");
                }
            }
            break;
        }
    }
}
