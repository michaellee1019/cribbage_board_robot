#include <Coordinator.hpp>
#include <ButtonInputRules.hpp>
#include <ErrorHandler.hpp>
#include <GameState.hpp>
#include <GameRules.hpp>
#include <ControllerRules.hpp>
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
#include <VisualFeedbackRules.hpp>
#include <utils.hpp>
#if defined(SCOREBOT_USB_CONTROL)
#include <UsbControlRules.hpp>
#endif

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
    scorebot::kPlayerPlusFiveButtonMask == (1u << ButtonGrid::pin3));
static_assert(
    scorebot::kPlayerPlusOneButtonMask == (1u << ButtonGrid::pin2));
static_assert(scorebot::kPlayerOkButtonMask == (1u << ButtonGrid::okPin));
static_assert(
    scorebot::kLeaderboardAddButtonMask == (1u << ButtonGrid::negone));
static_assert(
    scorebot::kLeaderboardNegativeOneButtonMask ==
    (1u << ButtonGrid::pin3));
static_assert(
    scorebot::kLeaderboardPlusOneButtonMask == (1u << ButtonGrid::add));
static_assert(
    scorebot::kLeaderboardOkButtonMask == (1u << ButtonGrid::pin2));

constexpr BoardRole kPlayers[] = {
    BoardRole::Player_Red,
    BoardRole::Player_Blue,
    BoardRole::Player_Green,
    BoardRole::Player_Yellow,
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
    rules.turnOrder = state.turnOrder;
    rules.completedTurns = state.completedTurns;
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
    state.turnOrder = rules.turnOrder;
    state.completedTurns = rules.completedTurns;
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
    document["session"] = state.sessionId;
    JsonArray controllers = document["controllers"].to<JsonArray>();
    JsonArray controlOps = document["controlOps"].to<JsonArray>();
    for (size_t i = 0; i < state.controllerNodes.size(); ++i) {
        controllers.add(state.controllerNodes[i]);
        controlOps.add(state.controllerOperations[i]);
    }
    document["term"] = state.term;
    document["version"] = state.version;
    document["leader"] = state.leaderId;
    document["started"] = state.gameStarted;
    document["turn"] = static_cast<uint8_t>(state.whosTurn);
    document["order"] = state.turnOrder;

    JsonArray scores = document["scores"].to<JsonArray>();
    JsonArray operations = document["operations"].to<JsonArray>();
    JsonArray turns = document["turns"].to<JsonArray>();
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        scores.add(scoreFor(state, role));
        operations.add(operationFor(state, role));
        turns.add(state.completedTurns[index]);
    }
    document["connected"] = state.connectedMask;
    document["local"] = state.localControlMask;
    document["enabled"] = state.lobbyEnabledMask;
    document["explicit"] = state.lobbyExplicitMask;
    document["roster"] = state.rosterMask;
    if (state.gameStarted && state.sleepingMask != 0) document["sleep"] = state.sleepingMask;

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
    if (document["leader"].as<uint32_t>() != state.leaderId ||
        !document["session"].is<uint32_t>() || document["session"].as<uint32_t>() != state.sessionId) {
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
    const JsonVariantConst sleep = document["sleep"];
    const uint8_t sleepingMask = sleep.as<uint8_t>();
    const BoardRole turn = static_cast<BoardRole>(document["turn"] | 0);
    const JsonVariantConst order = document["order"];
    const JsonVariantConst turns = document["turns"];
    const bool turnHistoryKnown = !turns.isUnbound();
    if ((connectedMask & ~0x0fu) != 0 || (localControlMask & ~0x0fu) != 0 ||
        (lobbyEnabledMask & ~0x0fu) != 0 ||
        (lobbyExplicitMask & ~0x0fu) != 0 ||
        (localControlMask & ~lobbyEnabledMask) != 0 ||
        (rosterMask & ~0x0fu) != 0 ||
        ((localControlMask & ~rosterMask) != 0 && gameStarted) ||
        (!sleep.isUnbound() && (!sleep.is<uint8_t>() || !gameStarted ||
                               (sleepingMask & ~rosterMask) != 0)) ||
        (turn != BoardRole::Unknown && !isPlayer(turn)) ||
        (!order.isUnbound() && (!order.is<uint8_t>() || !scorebot::isTurnOrder(order.as<uint8_t>()))) ||
        (turnHistoryKnown && (!turns.is<JsonArrayConst>() || turns.size() != std::size(kPlayers)))) {
        return SnapshotDecodeResult::Invalid;
    }
    scorebot::TurnCompletions completedTurns{};
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        if (!scores[index].is<int32_t>() || !operations[index].is<uint32_t>() ||
            (turnHistoryKnown && !turns[index].is<uint16_t>())) {
            return SnapshotDecodeResult::Invalid;
        }
        completedTurns[index] = turns[index].as<uint16_t>();
    }
    if (!document["controllers"].is<JsonArray>() || !document["controlOps"].is<JsonArray>() ||
        document["controllers"].size() != 4 || document["controlOps"].size() != 4) return SnapshotDecodeResult::Invalid;
    for (size_t i = 0; i < 4; ++i) {
        if (!document["controllers"][i].is<uint32_t>() || !document["controlOps"][i].is<uint32_t>())
            return SnapshotDecodeResult::Invalid;
    }
    switch (scorebot::compareRevision({term, version}, {state.term, state.version})) {
        case scorebot::RevisionOrder::Older:
            return SnapshotDecodeResult::Older;
        case scorebot::RevisionOrder::Equal:
            return SnapshotDecodeResult::Equal;
        case scorebot::RevisionOrder::Newer:
            break;
    }

    for (size_t i = 0; i < 4; ++i) {
        state.controllerNodes[i] = document["controllers"][i].as<uint32_t>();
        state.controllerOperations[i] = document["controlOps"][i].as<uint32_t>();
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
    state.sleepingMask = sleepingMask;
    state.turnOrder = order | scorebot::kDefaultTurnOrder;
    state.completedTurns = completedTurns;
    if (!state.turnHistoryKnown && turnHistoryKnown) {
        state.draftTurnBaseline = completedTurns;
    }
    state.turnHistoryKnown = turnHistoryKnown;
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        const BoardRole role = kPlayers[index];
        scoreFor(state, role) = scores[index].as<int32_t>();
        operationFor(state, role) = operations[index].as<uint32_t>();
    }
    state.leaderless = false;
    return SnapshotDecodeResult::Applied;
}

void setPlayerDisplay(const GameState& state, Coordinator* coordinator) {
    const BoardRole role = coordinator->myRole();
    if (!isPlayer(role) || !coordinator->normalUiWritesAllowed()) {
        return;
    }
    switch (state.playerDisplayMode(role)) {
        case scorebot::PlayerDisplayMode::Delta:
            coordinator->display1.print(strFormat("%d", state.myScore));
            break;
        case scorebot::PlayerDisplayMode::Pairing:
            coordinator->display1.print(strFormat("P %02u", coordinator->ble.pairingId()));
            break;
        case scorebot::PlayerDisplayMode::Turn: {
            const auto display = scorebot::activeTurnFrame(
                scoreFor(state, role), millis() - state.turnStatusStartedMs,
                scorebot::ScoreDisplayStyle::Framed);
            coordinator->display1.print(display.data());
            break;
        }
        case scorebot::PlayerDisplayMode::Idle:
            coordinator->display1.print("----");
            break;
        case scorebot::PlayerDisplayMode::WaitingScore: {
            const auto display = scorebot::scoreDisplayFrame(
                scoreFor(state, role),
                (millis() - state.turnStatusStartedMs) / scorebot::kPlayerScoreScrollMs,
                scorebot::ScoreDisplayStyle::Framed);
            coordinator->display1.print(display.data());
            break;
        }
    }
}

void setLeaderboardDisplays(GameState& state, Coordinator* coordinator) {
    if (coordinator->myRole() != BoardRole::Leader) {
        return;
    }
    const std::array<HT16Display*, 4> displays = {
        &coordinator->display1, &coordinator->display2, &coordinator->display3, &coordinator->display4};
    const bool editingOrder = !state.gameStarted && state.editingTurnOrder;
    const size_t selectedIndex = roleIndex(state.selectedLeaderboardRole);
    bool selectedLocalPrompt = false;
    if (!state.gameStarted && !editingOrder && selectedIndex < std::size(kPlayers)) {
        const uint8_t selectedBit = static_cast<uint8_t>(1u << selectedIndex);
        selectedLocalPrompt =
            (state.localControlMask & selectedBit) != 0;
    }
    coordinator->setLeaderboardSelectedDisplay(
        !editingOrder && selectedIndex < std::size(kPlayers)
            ? static_cast<int8_t>(selectedIndex) : -1,
        !state.gameStarted, selectedLocalPrompt);
    // Publish the authoritative turn light before writing display content. A
    // newly rendered turn prompt must use the current turn's color.
    if (state.gameStarted && isPlayer(state.whosTurn)) {
        coordinator->setLeaderboardTurnColor(
            getRoleStyle(state.whosTurn).color);
    } else {
        coordinator->setLeaderboardTurnColor(0x000000);
    }
    if (!coordinator->normalUiWritesAllowed()) {
        return;
    }
    std::array<const char*, scorebot::kPlayerCount> orderLabels{};
    if (editingOrder) {
        constexpr const char* ordinals[] = {"1ST", "2ND", "3RD", "4TH"};
        const uint8_t participants = scorebot::lobbyParticipantMask(toRules(state));
        size_t position = 0;
        for (const auto player : scorebot::decodeTurnOrder(state.lobbyTurnOrder)) {
            if ((participants & scorebot::playerBit(player)) != 0) {
                orderLabels[scorebot::playerIndex(player)] = ordinals[position++];
            }
        }
    }
    const uint32_t now = millis();
    const uint32_t turnElapsed = now - state.turnStatusStartedMs;
    const uint32_t frame = scorebot::turnStatusFrame(turnElapsed);
    const bool showSavedScore = scorebot::turnStatusShowsScore(turnElapsed);
    for (size_t index = 0; index < std::size(kPlayers); ++index) {
        if (orderLabels[index] != nullptr) {
            displays[index]->print(orderLabels[index]);
            continue;
        }
        const BoardRole role = kPlayers[index];
        const auto printScore = [&](uint32_t scoreFrame) {
            const auto text = scorebot::scoreDisplayFrame(
                scoreFor(state, role), scoreFrame, scorebot::ScoreDisplayStyle::Plain);
            displays[index]->print(text.data());
        };
        const bool connected = (state.connectedMask & (1u << index)) != 0;
        const bool locallyControlled = (state.localControlMask & (1u << index)) != 0;
        const bool lobbyEnabled = (state.lobbyEnabledMask & (1u << index)) != 0;
        const bool sleeping = (state.sleepingMask & (1u << index)) != 0;
        const bool inRoster = (state.rosterMask & (1u << index)) != 0;
        if (state.gameStarted && inRoster && state.leaderboardDeltas[index] != 0) {
            displays[index]->print(strFormat("%d", state.leaderboardDeltas[index]));
            if (state.whosTurn == role) {
                DEBUG_PRINTF("LB draft t=%lu slot=%u delta=%ld brightness=%u\n",
                    static_cast<unsigned long>(millis()), static_cast<unsigned>(index),
                    static_cast<long>(state.leaderboardDeltas[index]),
                    static_cast<unsigned>(displays[index]->currentBrightness()));
            }
            continue;
        }
        switch (scorebot::leaderboardDisplayMode(
            state.gameStarted, connected, inRoster, sleeping,
            locallyControlled, lobbyEnabled,
            state.whosTurn == role)) {
            case scorebot::LeaderboardDisplayMode::Score:
                printScore(frame);
                break;
            case scorebot::LeaderboardDisplayMode::LobbyName:
                displays[index]->print(getRoleStyle(role).name);
                break;
            case scorebot::LeaderboardDisplayMode::Pairing:
                displays[index]->print(strFormat("P %02u", coordinator->ble.pairingId()));
                break;
            case scorebot::LeaderboardDisplayMode::LocalControl:
                if (!editingOrder && index == selectedIndex) {
                    const auto prompt = scorebot::leaderboardLocalPromptFrame(0);
                    displays[index]->print(prompt.data());
                } else {
                    displays[index]->print("LOCL");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Off:
                displays[index]->print(!state.gameStarted && (millis() / 700) % 2
                    ? strFormat("P %02u", coordinator->ble.pairingId()) : String("OFF "));
                break;
            case scorebot::LeaderboardDisplayMode::Turn: {
                const auto display = scorebot::activeTurnFrame(
                    scoreFor(state, role), turnElapsed, scorebot::ScoreDisplayStyle::Plain);
                displays[index]->print(display.data());
                DEBUG_PRINTF("LB redraw t=%lu due=%lu frame=%lu slot=%u text=\"%s\" brightness=%u\n",
                    static_cast<unsigned long>(millis()),
                    static_cast<unsigned long>(state.turnStatusStartedMs +
                                               frame * scorebot::kTurnStatusAlternateMs),
                    static_cast<unsigned long>(frame), static_cast<unsigned>(index),
                    display.data(), static_cast<unsigned>(displays[index]->currentBrightness()));
                break;
            }
            case scorebot::LeaderboardDisplayMode::Rejoining:
                if (showSavedScore) {
                    printScore(frame / 2);
                } else {
                    displays[index]->print(strFormat("P %02u", coordinator->ble.pairingId()));
                }
                break;
            case scorebot::LeaderboardDisplayMode::Sleeping:
                if (state.gameStarted && inRoster && showSavedScore) {
                    printScore(frame / 2);
                } else {
                    displays[index]->print("ZZZZ");
                }
                break;
            case scorebot::LeaderboardDisplayMode::Blank:
                displays[index]->clear();
                break;
        }
    }
    if (state.gameStarted) state.lastTurnStatusFrame = frame;
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
    state.editingTurnOrder = false;
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
    if (coordinator->ble.sendTo(coordinator->ble.authorityNodeId(), encoded)) {
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

void editPlayerDraft(GameState& state, int32_t delta) {
    if (state.myScore == 0) {
        state.draftTurnBaseline = state.completedTurns;
    }
    addDelta(state.myScore, delta);
}

void sendControllerCommand(GameState* state, Coordinator* coordinator,
                           scorebot::ControllerCommand command, int32_t value = 0, bool passesTurn = false) {
    if (coordinator->ble.isAuthority() || !coordinator->ble.hasLeader() || state->leaderless ||
        state->pendingOperation != 0) return;
    JsonDocument doc;
    doc["type"] = "control";
    doc["protocol"] = scorebot::kWireProtocolVersion;
    doc["from"] = coordinator->ble.getMyPeerId();
    doc["op"] = state->nextOperationId();
    doc["game"] = state->gameId;
    doc["term"] = state->term;
    doc["command"] = static_cast<uint8_t>(command);
    doc["player"] = static_cast<uint8_t>(toRulePlayer(state->selectedLeaderboardRole));
    doc["value"] = value;
    doc["pass"] = passesTurn;
    String encoded;
    serializeJson(doc, encoded);
    if (!coordinator->ble.sendTo(coordinator->ble.authorityNodeId(), encoded)) return;
    state->pendingOperation = doc["op"].as<uint32_t>();
    state->pendingMessage = encoded;
    state->pendingSentMs = millis();
    if (command == scorebot::ControllerCommand::Score) {
        state->pendingControllerRole = state->selectedLeaderboardRole;
        state->pendingScore = value;
        const size_t index = roleIndex(state->selectedLeaderboardRole);
        if (index < 4) state->leaderboardDeltas[index] = 0;
    }
    state->refreshDisplays(coordinator);
}

void sendLeaderboardOperation(
    GameState* state, Coordinator* coordinator, scorebot::LeaderboardAction action) {
    if (!coordinator->ble.isAuthority()) {
        const size_t index = roleIndex(state->selectedLeaderboardRole);
        if (index < 4) sendControllerCommand(state, coordinator, scorebot::ControllerCommand::Score,
            state->leaderboardDeltas[index], action == scorebot::LeaderboardAction::Ok);
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    const scorebot::LeaderboardActionResult result = scorebot::applyLeaderboardAction(
        rules, state->leaderboardDeltas,
        toRulePlayer(state->selectedLeaderboardRole), action);
    if (result == scorebot::LeaderboardActionResult::Accepted) {
        coordinator->rotaryEncoder.reset();
        fromRules(*state, rules);
        commit(*state, coordinator);
    } else if (result == scorebot::LeaderboardActionResult::Rejected) {
        state->refreshDisplays(coordinator);
    }
}

void clearTransientInput(GameState& state) {
    state.myScore = 0;
    state.editingTurnOrder = false;
    state.lobbyTurnOrder = scorebot::kDefaultTurnOrder;
    state.leaderboardDeltas.fill(0);
    state.stagedButtonScores.clear();
    state.pendingOperation = 0;
    state.pendingScore = 0;
    state.pendingPass = false;
    state.pendingMessage = "";
    state.pendingSentMs = 0;
    state.localOperation = 0;
}

void clearTurnState(GameState& state) {
    state.turnOrder = scorebot::kDefaultTurnOrder;
    state.completedTurns.fill(0);
    state.turnHistoryKnown = false;
}

void clearCurrentBoardState(GameState* state, Coordinator* coordinator) {
    Preferences preferences;
    if (!coordinator->waitForStatePersistence(5000)) {
        FATAL_ERROR(
            ErrorCode::STATE_PERSIST_FAILED,
            "drain state persistence before reset");
    }
    const bool leaderboard = coordinator->ble.isAuthority();
    // Establish the exclusive dispatcher/BLE gate while the state mutex is
    // still held, before either the authoritative snapshot or local NVS can
    // change underneath a newly arriving event.
    coordinator->requestRestart(leaderboard);
    if (leaderboard) {
        const uint32_t nextTerm = state->term + 1;
        const uint32_t nextGame = state->gameId + 1;
        state->scores.fill(0);
        state->lastOperation.fill(0);
        state->controllerNodes.fill(0);
        state->controllerOperations.fill(0);
        state->connectedMask = 0;
        state->localControlMask = 0;
        state->lobbyEnabledMask = 0;
        state->lobbyExplicitMask = 0;
        state->sleepingMask = 0;
        state->rosterMask = 0;
        clearTurnState(*state);
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
            state->gameId, state->term, state->leaderId, state->sessionId).toJson();
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
    coordinator->ble.clearPairing();
    coordinator->finishRestartPreparation();
}

void startMaintenance(
    GameState* state, Coordinator* coordinator,
    scorebot::MaintenanceAction action, uint16_t pressedMask) {
    state->stagedButtonScores.clear();
    state->invalidButtonGesture = false;
    state->maintenanceAction = action;
    state->maintenanceStartedMs = millis();
    const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
    state->suppressedButtonMask |=
        scorebot::maintenanceChordMask(action, leaderboard, coordinator->ble.getMyPeerId());
    state->heldButtonMask = pressedMask;
    coordinator->noteInteraction();
    coordinator->startMaintenanceUi(action, state->maintenanceStartedMs);
}

void cancelMaintenance(GameState* state, Coordinator* coordinator, uint16_t pressedMask) {
    state->suppressHeldButtons(pressedMask);
    state->maintenanceAction = scorebot::MaintenanceAction::None;
    state->maintenanceStartedMs = 0;
    coordinator->cancelMaintenanceUi();
}

void applySingleButton(GameState* state, uint8_t pin, Coordinator* coordinator,
                       BoardRole scoreTarget) {
    const BoardRole role = coordinator->myRole();
    const scorebot::ButtonAction action = scorebot::buttonActionForPin(
        role == BoardRole::Leader, pin, coordinator->ble.getMyPeerId());
    if (action == scorebot::ButtonAction::None) {
        return;
    }
    if (coordinator->ble.pairingUiActive()) {
        if (action == scorebot::ButtonAction::Ok) coordinator->ble.confirmPairing();
        else if (action == scorebot::ButtonAction::Add) coordinator->ble.chooseColor();
        else coordinator->ble.rotatePairing(action == scorebot::ButtonAction::NegativeOne ? -1 : 1);
        return;
    }
    if (role == BoardRole::Leader) {
        if (!coordinator->ble.isAuthority() && (!coordinator->ble.hasLeader() || state->leaderless)) return;
        if (!state->gameStarted) {
            if (action == scorebot::ButtonAction::Add) {
                state->editingTurnOrder = !state->editingTurnOrder &&
                    scorebot::lobbyParticipantMask(toRules(*state)) != 0;
                state->refreshDisplays(coordinator);
                return;
            }
            if (action == scorebot::ButtonAction::Ok) {
                if (!coordinator->ble.isAuthority()) {
                    sendControllerCommand(state, coordinator, scorebot::ControllerCommand::Start,
                                          state->lobbyTurnOrder);
                    return;
                }
                scorebot::Snapshot rules = toRules(*state);
                if (scorebot::start(rules, state->lobbyTurnOrder)) {
                    fromRules(*state, rules);
                    state->lobbyModeDirty = false;
                    coordinator->ble.freezeRoster(state->rosterMask);
                    commit(*state, coordinator);
                }
            }
            return;
        }

        if (action == scorebot::ButtonAction::Add ||
            action == scorebot::ButtonAction::Ok) {
            sendLeaderboardOperation(
                state, coordinator,
                action == scorebot::ButtonAction::Add
                    ? scorebot::LeaderboardAction::Add
                    : scorebot::LeaderboardAction::Ok);
            return;
        }
        const size_t index = roleIndex(scoreTarget);
        if (index >= std::size(kPlayers) ||
            !scorebot::isLocallyControllable(
                toRules(*state), toRulePlayer(scoreTarget))) {
            state->refreshDisplays(coordinator);
            return;
        }
        if (action == scorebot::ButtonAction::NegativeOne) {
            addDelta(state->leaderboardDeltas[index], -1);
        } else if (action == scorebot::ButtonAction::PlusOne) {
            addDelta(state->leaderboardDeltas[index], 1);
        } else if (action == scorebot::ButtonAction::PlusFive) {
            addDelta(state->leaderboardDeltas[index], 5);
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
        editPlayerDraft(*state, -1);
    } else if (action == scorebot::ButtonAction::PlusOne) {
        editPlayerDraft(*state, 1);
    } else if (action == scorebot::ButtonAction::PlusFive) {
        editPlayerDraft(*state, 5);
    } else if (action == scorebot::ButtonAction::Ok) {
        sendPlayerOperation(state, coordinator, true);
        return;
    }
    setPlayerDisplay(*state, coordinator);
}

bool canApplyButtonScore(const GameState& state, Coordinator* coordinator) {
    return state.gameStarted &&
           (coordinator->ble.isAuthority() ||
            (!state.leaderless && coordinator->ble.hasLeader()));
}

void drainStagedButtonScores(GameState* state, Coordinator* coordinator, uint32_t nowUs) {
    GameState::StagedButtonScore score{};
    while (state->stagedButtonScores.pop(nowUs, score)) {
        if (score.game == state->gameId && score.session == state->sessionId &&
            score.pairing == coordinator->ble.pairingUiActive() &&
            (score.pairing || (score.canScore && canApplyButtonScore(*state, coordinator)))) {
            applySingleButton(state, score.pin, coordinator, score.target);
        }
    }
}

bool encoderCommitAllowed(GameState* state, Coordinator* coordinator) {
    if (!coordinator->buttonGrid.interruptAsserted()) {
        drainStagedButtonScores(state, coordinator, micros());
    }
    if (state->stagedButtonScores.empty()) return true;
    state->stagedButtonScores.clear();
    coordinator->flashInvalidGesture();
    return false;
}

void onButtonGridEvent(GameState* state, Coordinator* coordinator,
                       const ButtonGrid::Interrupt* injected = nullptr) {
    const ButtonGrid::Interrupt interrupt = injected ? *injected : coordinator->buttonGrid.consumeInterrupt();
    const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
    const uint32_t node = coordinator->ble.getMyPeerId();
    const uint16_t commitButtons = scorebot::otaChordMask(leaderboard);
    const uint16_t scoreButtons = scorebot::resetChordMask(leaderboard, node) |
        (leaderboard ? 0 : (1u << scorebot::playerPlusFivePinForBoard(node)));
    const uint16_t buttons = commitButtons | scoreButtons;
    const uint16_t current = interrupt.pressed & buttons;
    const uint16_t previous = state->heldButtonMask;
    const uint16_t previouslySuppressed = state->suppressedButtonMask;
    const uint16_t captured = interrupt.pin < 16
        ? static_cast<uint16_t>(~interrupt.captured) & buttons : 0;
    const scorebot::ButtonEdges edges = scorebot::buttonEdges(
        previous, current, captured, interrupt.capturedPresses & buttons,
        interrupt.capturedOverlap);
    DEBUG_PRINTF(
        "Button grid: edge_us=%lu role=%u pin=%u captured=0x%04x held=0x%02x press=0x%02x release=0x%02x overlap=%d\n",
        static_cast<unsigned long>(interrupt.firstEdgeUs),
        static_cast<unsigned>(coordinator->myRole()),
        static_cast<unsigned>(interrupt.pin),
        static_cast<unsigned>(interrupt.captured),
        static_cast<unsigned>(current),
        static_cast<unsigned>(edges.pressed), static_cast<unsigned>(edges.completed),
        edges.overlap);

#if defined(SCOREBOT_BUTTON_DIAGNOSTIC)
    // A passive diagnostic must report both edges without letting ADD, OK, or
    // a maintenance chord mutate the persisted game while wiring is probed.
    state->heldButtonMask = current;
    state->suppressedButtonMask = 0;
    return;
#endif

    if (edges.pressed != 0 || edges.completed != 0 || edges.overlap) {
        coordinator->noteInteraction();
    }
    const uint32_t now = millis();
    const uint32_t observedUs = micros();
    if (injected) {
        // The USB harness sends explicit, complete taps and has no overlap or
        // timing semantics. Keep its acknowledgement synchronous.
        applySingleButton(state, interrupt.pin, coordinator, state->selectedLeaderboardRole);
        return;
    }

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
        (void)coordinator->consumePrinterUiInput();
        return;
    }

    if (state->maintenanceAction != scorebot::MaintenanceAction::None) {
        const scorebot::MaintenanceAction next =
            scorebot::maintenanceActionTransition(
                state->maintenanceAction, current, leaderboard, node);
        if (next != state->maintenanceAction) {
            // The four-button leaderboard gesture can be assembled through
            // either two-button maintenance chord. Start its safety interval
            // when the fourth button actually arrives.
            startMaintenance(state, coordinator, next, current);
        } else if (!scorebot::maintenanceChordHeld(
                       state->maintenanceAction, current, leaderboard, node)) {
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
            scorebot::MaintenanceAction::None, current, leaderboard, node);
    const uint32_t edgeUs = interrupt.firstEdgeUs != 0 ? interrupt.firstEdgeUs : observedUs;
    state->stagedButtonScores.discardBouncedReleases(edgeUs, edges.pressed & scoreButtons);
    drainStagedButtonScores(state, coordinator, edgeUs);
    if (chord != scorebot::MaintenanceAction::None) {
        startMaintenance(state, coordinator, chord, current);
        return;
    }

    state->heldButtonMask = current;
    const bool rejected = scorebot::rejectButtonGesture(
        edges, scoreButtons, commitButtons, !state->stagedButtonScores.empty());
    if (rejected || state->invalidButtonGesture) {
        if (!state->invalidButtonGesture) coordinator->flashInvalidGesture();
        state->invalidButtonGesture = current != 0;
        state->stagedButtonScores.clear();
        for (uint8_t pin = 0; pin <= ButtonGrid::okPin; ++pin) {
            if ((edges.completed & commitButtons & (1u << pin)) != 0) {
                (void)state->buttonReleaseDebounce.accept(pin, now);
            }
        }
        return;
    }

    // Scores remain reversible until the guard closes. ADD/OK resolve on
    // release only after their press has passed gesture classification.
    for (uint8_t pin = 0; pin <= ButtonGrid::okPin; ++pin) {
        const uint16_t bit = 1u << pin;
        if ((edges.completed & bit) == 0) continue;
        if ((scoreButtons & bit) == 0) {
            if (state->buttonReleaseDebounce.accept(pin, now)) {
                applySingleButton(state, pin, coordinator, state->selectedLeaderboardRole);
            }
            continue;
        }
        const GameState::StagedButtonScore score{
            state->gameId, state->sessionId, state->selectedLeaderboardRole, pin,
            coordinator->ble.pairingUiActive(), canApplyButtonScore(*state, coordinator)};
        if (!state->stagedButtonScores.stage(observedUs, score, pin)) {
            state->stagedButtonScores.clear();
            coordinator->flashInvalidGesture();
            return;
        }
    }
}

void onButtonPress(GameState* state, const ButtonPressEvent& event, Coordinator* coordinator) {
    const BoardRole role = coordinator->myRole();
    if (event.buttonName == ButtonName::RotaryEncoder && coordinator->ble.pairingUiActive()) {
        const bool pressed = coordinator->rotaryEncoder.pressed();
        const int32_t delta = coordinator->rotaryEncoder.delta();
        if (pressed) {
            state->rotaryPressStartedMs.store(millis());
        } else if (state->rotaryPressStartedMs.exchange(0) != 0) {
            if (encoderCommitAllowed(state, coordinator)) coordinator->ble.confirmPairing();
        } else {
            coordinator->ble.rotatePairing(delta);
        }
        coordinator->noteInteraction();
        return;
    }
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
            if (state->rotaryPressStartedMs.load() == 0) {
                state->rotaryPressStartedMs.store(millis());
            }
            return;
        }

        if (state->rotaryPressStartedMs.exchange(0) != 0) {
            if (role == BoardRole::Leader) {
                if (!state->editingTurnOrder) {
                    cycleLeaderboardSelection(*state, coordinator);
                }
            } else if (encoderCommitAllowed(state, coordinator)) {
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
                scorebot::Snapshot rules = toRules(*state);
                if (state->editingTurnOrder) {
                    const uint8_t participants = scorebot::lobbyParticipantMask(rules);
                    state->lobbyTurnOrder = scorebot::rotatedTurnOrder(
                        state->lobbyTurnOrder, participants, delta);
                    state->refreshDisplays(coordinator);
                    return;
                }
                const scorebot::Player player =
                    toRulePlayer(state->selectedLeaderboardRole);
                const scorebot::LobbyMode next = scorebot::rotatedLobbyMode(
                    scorebot::lobbyModeFor(rules, player), delta);
                if (!coordinator->ble.isAuthority()) {
                    sendControllerCommand(state, coordinator, scorebot::ControllerCommand::Lobby, static_cast<int32_t>(next));
                    return;
                }
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
            editPlayerDraft(*state, delta);
        }
        setPlayerDisplay(*state, coordinator);
        return;
    }

    onButtonGridEvent(state, coordinator);
}

void onControllerMessage(GameState* state, const Event& event, Coordinator* coordinator, const JsonDocument& doc) {
    const uint32_t node = event.messageReceived.peerId;
    if (!coordinator->ble.isAuthority() || !coordinator->ble.isControllerPeer(node) ||
        !doc["protocol"].is<uint16_t>() || doc["protocol"].as<uint16_t>() != scorebot::kWireProtocolVersion ||
        !doc["from"].is<uint32_t>() || doc["from"].as<uint32_t>() != node ||
        !doc["game"].is<uint32_t>() || doc["game"].as<uint32_t>() != state->gameId ||
        !doc["term"].is<uint32_t>() || doc["term"].as<uint32_t>() != state->term ||
        !doc["op"].is<uint32_t>() || !doc["command"].is<uint8_t>() ||
        !doc["player"].is<uint8_t>() || !doc["value"].is<int32_t>() || !doc["pass"].is<bool>()) {
        if (coordinator->ble.isAuthority()) rejectAndResync(*state, coordinator, node);
        return;
    }
    const uint8_t command = doc["command"].as<uint8_t>();
    const int32_t value = doc["value"].as<int32_t>();
    if (command > static_cast<uint8_t>(scorebot::ControllerCommand::Lobby) ||
        (command == static_cast<uint8_t>(scorebot::ControllerCommand::Lobby) && (value < 0 || value > 2)) ||
        (command == static_cast<uint8_t>(scorebot::ControllerCommand::Start) && !scorebot::isTurnOrder(value))) return;
    scorebot::ControllerAction action{node, doc["op"].as<uint32_t>(),
        static_cast<scorebot::ControllerCommand>(command), static_cast<scorebot::Player>(doc["player"].as<uint8_t>()),
        value, doc["pass"].as<bool>(), command == static_cast<uint8_t>(scorebot::ControllerCommand::Lobby)
            ? static_cast<scorebot::LobbyMode>(value) : scorebot::LobbyMode::Off,
        command == static_cast<uint8_t>(scorebot::ControllerCommand::Start)
            ? static_cast<uint8_t>(value) : scorebot::kDefaultTurnOrder};
    scorebot::Snapshot rules = toRules(*state);
    if (scorebot::applyController(rules, state->controllerNodes, state->controllerOperations, action)) {
        coordinator->noteRemoteInteraction();
        fromRules(*state, rules);
        if (state->gameStarted) coordinator->ble.freezeRoster(state->rosterMask);
        commit(*state, coordinator);
    } else rejectAndResync(*state, coordinator, node);
}

void onPlayerMessage(GameState* state, const Event& event, Coordinator* coordinator) {
    if (!coordinator->ble.isAuthority()) {
        return;
    }
    const String json = event.messageReceived.message;
    if (!PlayerMessage::isPlayerMessage(json)) {
        return;
    }
    const PlayerMessage message = PlayerMessage::fromJson(json);
    const uint32_t sender = event.messageReceived.peerId;
    const BoardRoleConfig& senderConfig = getRoleStyle(coordinator->ble.peerRole(sender));
    if (!isPlayer(senderConfig.role) || message.fromNodeId != sender ||
        message.gameId != state->gameId || !state->gameStarted) {
        rejectAndResync(*state, coordinator, sender);
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    const uint32_t previousVersion = rules.version;
    const scorebot::ApplyResult result = scorebot::apply(
        rules, {toRulePlayer(senderConfig.role), message.score, message.turnPassed, message.operationId});
    if (result == scorebot::ApplyResult::Accepted ||
        result == scorebot::ApplyResult::AcceptedWithoutTurnChange ||
        rules.version != previousVersion) {
        coordinator->noteRemoteInteraction();
        // Valid rejected operations are recorded as consumed so a player can
        // distinguish rejection from packet loss and stop retrying them.
        fromRules(*state, rules);
        commit(*state, coordinator);
    } else {
        rejectAndResync(*state, coordinator, sender);
    }
}

void onSessionActivityMessage(
    GameState* state, const Event& event, Coordinator* coordinator) {
    if (!coordinator->ble.isAuthority()) {
        return;
    }
    const PlayerActivityMessage message =
        PlayerActivityMessage::fromJson(event.messageReceived.message);
    const uint32_t sender = event.messageReceived.peerId;
    if (message.fromNodeId != sender || message.gameId != state->gameId) {
        return;
    }
    const uint8_t senderBit = scorebot::playerBit(
        toRulePlayer(coordinator->ble.peerRole(sender)));
    const bool connectedPlayer = (state->connectedMask & senderBit) != 0 &&
        (!state->gameStarted || (state->rosterMask & senderBit) != 0);
    if (!connectedPlayer && !coordinator->ble.isControllerPeer(sender)) {
        return;
    }
    DEBUG_PRINTF("Session activity: from=%08lx game=%lu\n",
                 static_cast<unsigned long>(sender),
                 static_cast<unsigned long>(message.gameId));
    coordinator->noteRemoteInteraction();
}

void onLeaderSleepGrant(const GameState& state, const Event& event,
                        Coordinator* coordinator, const JsonDocument& message) {
    const uint32_t sender = event.messageReceived.peerId;
    if (state.leaderless || !coordinator->ble.hasLeader() ||
        !coordinator->ble.acceptsLeader(sender) ||
        !message["protocol"].is<uint16_t>() ||
        message["protocol"].as<uint16_t>() != scorebot::kWireProtocolVersion ||
        !message["leader"].is<uint32_t>() || message["leader"].as<uint32_t>() != sender ||
        !message["session"].is<uint32_t>() || message["session"].as<uint32_t>() != state.sessionId ||
        !message["game"].is<uint32_t>() || message["game"].as<uint32_t>() != state.gameId ||
        !message["term"].is<uint32_t>() || message["term"].as<uint32_t>() != state.term ||
        !message["ready"].is<bool>()) {
        return;
    }
    coordinator->observeSessionSleepGrant(message["ready"].as<bool>());
}

void onPlayerSleepMessage(
    GameState* state, const Event& event, Coordinator* coordinator) {
    if (!coordinator->ble.isAuthority()) {
        return;
    }
    const PlayerSleepMessage message =
        PlayerSleepMessage::fromJson(event.messageReceived.message);
    const uint32_t sender = event.messageReceived.peerId;
    const BoardRole senderRole = getRoleStyle(coordinator->ble.peerRole(sender)).role;
    const scorebot::Player player = toRulePlayer(senderRole);
    if (!scorebot::isPlayer(player) || message.fromNodeId != sender ||
        !scorebot::isConnected(toRules(*state), player)) {
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    scorebot::markSleeping(rules, player);
    fromRules(*state, rules);
    DEBUG_PRINTF("Player sleeping: from=%08lx\n",
                 static_cast<unsigned long>(sender));
    state->refreshDisplays(coordinator);
}

void onLeaderResetMessage(
    GameState* state, const Event& event, Coordinator* coordinator) {
    const uint32_t sender = event.messageReceived.peerId;
    if (!coordinator->ble.acceptsLeader(sender)) {
        return;
    }
    const LeaderResetMessage message =
        LeaderResetMessage::fromJson(event.messageReceived.message);
    JsonDocument resetEnvelope;
    deserializeJson(resetEnvelope, event.messageReceived.message);
    if (!resetEnvelope["session"].is<uint32_t>() || resetEnvelope["session"].as<uint32_t>() != state->sessionId) return;
    if (!scorebot::mayAcceptLeaderMessage(coordinator->ble.isAuthority(), coordinator->ble.authorityNodeId(), sender, message.leaderId)) {
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

    coordinator->observeSessionSleepGrant(false);

    state->scores.fill(0);
    state->lastOperation.fill(0);
    state->controllerNodes.fill(0);
    state->controllerOperations.fill(0);
    state->connectedMask = 0;
    state->localControlMask = 0;
    state->lobbyEnabledMask = 0;
    state->lobbyExplicitMask = 0;
    state->sleepingMask = 0;
    state->rosterMask = 0;
    clearTurnState(*state);
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
    if (!coordinator->ble.isAuthority()) {
        return;
    }
    const scorebot::Player player =
        toRulePlayer(coordinator->ble.peerRole(event.newPeer.peerId));
    scorebot::Snapshot rules = toRules(*state);
    scorebot::clearSleeping(rules, player);
    fromRules(*state, rules);
    if (scorebot::connect(rules, player)) {
        fromRules(*state, rules);
        commit(*state, coordinator);
    } else {
        rejectAndResync(*state, coordinator, event.newPeer.peerId);
    }
}

void onLostPeer(GameState* state, const Event& event, Coordinator* coordinator) {
    if (!coordinator->ble.isAuthority()) {
        return;
    }
    scorebot::Snapshot rules = toRules(*state);
    if (scorebot::disconnect(rules, toRulePlayer(coordinator->ble.peerRole(event.lostPeer.peerId)))) {
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
      lastLobbyDisplayMs(0),
      pendingOperation(0),
      pendingScore(0),
      pendingPass(false),
      pendingMessage(),
      pendingSentMs(0),
      leaderboardDeltas{},
      selectedLeaderboardRole(BoardRole::Player_Red),
      lobbyModeDirty(false),
      lobbyModeChangedMs(0),
      heldButtonMask(0),
      suppressedButtonMask(0),
      maintenanceAction(scorebot::MaintenanceAction::None),
      maintenanceStartedMs(0),
      rotaryPressStartedMs(0) {}

void GameState::resumeAuthority(uint32_t identity) {
    scorebot::Snapshot rules = toRules(*this);
    scorebot::resumeAuthority(rules, term);
    fromRules(*this, rules);
    leaderId = identity;
    leaderless = false;
}

void GameState::selectSession(uint32_t leader, uint32_t session) {
    if (leaderId == leader && sessionId == session) return;
    leaderId = leader;
    sessionId = session;
    gameId = term = version = 0;
    scores.fill(0);
    lastOperation.fill(0);
    controllerNodes.fill(0);
    controllerOperations.fill(0);
    connectedMask = localControlMask = lobbyEnabledMask = lobbyExplicitMask = sleepingMask = rosterMask = 0;
    clearTurnState(*this);
    gameStarted = false;
    whosTurn = BoardRole::Unknown;
    leaderless = true;
    clearTransientInput(*this);
}

void GameState::restore(uint32_t leader, uint32_t session) {
    leaderId = leader;
    sessionId = session;
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

void GameState::refreshDisplays(Coordinator* coordinator) {
    if (coordinator->ble.pairingUiActive() || (!coordinator->ble.isAuthority() && !coordinator->ble.hasLeader())) return;
    setLeaderboardDisplays(*this, coordinator);
    setPlayerDisplay(*this, coordinator);
}

void GameState::refreshLeaderboardTurnFrame(Coordinator* coordinator) {
    if (!gameStarted || !coordinator->normalUiWritesAllowed() ||
        coordinator->ble.pairingUiActive() || !coordinator->ble.hasLeader()) return;

    const uint32_t now = millis();
    const uint32_t frame = scorebot::turnStatusFrame(now - turnStatusStartedMs);
    if (frame == lastTurnStatusFrame) return;
    DEBUG_PRINTF("LB tick t=%lu frame=%lu previous=%lu\n",
        static_cast<unsigned long>(now), static_cast<unsigned long>(frame),
        static_cast<unsigned long>(lastTurnStatusFrame));
    setLeaderboardDisplays(*this, coordinator);
}

scorebot::PlayerDisplayMode GameState::playerDisplayMode(BoardRole role) const {
    const bool isMyTurn = scorebot::playerTurnLocallyActive(whosTurn == role, pendingPass);
    return scorebot::playerDisplayMode(myScore, leaderless, gameStarted, isMyTurn);
}

void GameState::syncLeaderboardSelection(Coordinator* coordinator) {
    selectAuthoritativeTurn(*this, coordinator);
}

scorebot::LeaderboardBroadcast GameState::leaderboardBroadcast() const {
    if (!gameStarted) {
        return {};
    }
    return {scores, rosterMask, toRulePlayer(whosTurn)};
}

bool GameState::hasPendingLocalScore() const {
    if (myScore != 0) {
        return true;
    }
    return std::any_of(
        leaderboardDeltas.begin(), leaderboardDeltas.end(),
        [](int32_t delta) { return delta != 0; });
}

bool GameState::buttonGestureInProgress() const {
    return heldButtonMask != 0 || suppressedButtonMask != 0 || !stagedButtonScores.empty();
}

void GameState::suppressHeldButtons(uint16_t pressed) {
    heldButtonMask = pressed;
    suppressedButtonMask = pressed;
    invalidButtonGesture = false;
}

void GameState::heartbeat(Coordinator* coordinator) {
    const uint32_t now = millis();
    if (maintenanceAction != scorebot::MaintenanceAction::None) {
        const bool leaderboard = coordinator->myRole() == BoardRole::Leader;
        const uint16_t pressed = coordinator->buttonGrid.pressedMask();
        const scorebot::MaintenanceAction next =
            scorebot::maintenanceActionTransition(
                maintenanceAction, pressed, leaderboard, coordinator->ble.getMyPeerId());
        if (next != maintenanceAction) {
            // The hardware level can show the fourth button before its queued
            // interrupt reaches the dispatcher. Promote here too, so a RESET
            // or OTA hold can never complete underneath a physical PRINT
            // chord. PRINT receives a fresh five-second safety interval.
            startMaintenance(this, coordinator, next, pressed);
            return;
        }
        const uint16_t required = scorebot::maintenanceChordMask(
            maintenanceAction, leaderboard, coordinator->ble.getMyPeerId());
        if (!scorebot::maintenanceChordHeld(
                maintenanceAction, pressed, leaderboard, coordinator->ble.getMyPeerId())) {
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
    // A queued physical edge gets priority over publishing a staged score.
    if (!stagedButtonScores.empty() && !coordinator->buttonGrid.interruptAsserted()) {
        drainStagedButtonScores(this, coordinator, micros());
    }
    const BoardRole localRole = coordinator->myRole();
    if (gameStarted && myScore != 0 && turnHistoryKnown &&
        !buttonGestureInProgress() && rotaryPressStartedMs.load() == 0 &&
        scorebot::draftHasExpired(now, coordinator->lastLocalInteraction(),
            toRulePlayer(localRole), draftTurnBaseline, completedTurns)) {
        myScore = 0;
        DEBUG_PRINTF("Player draft expired after inactivity and another completed turn\n");
        refreshDisplays(coordinator);
    }
    constexpr uint32_t noAnimation = 0xffffffff;
    uint32_t frame = noAnimation;
    const uint32_t turnElapsed = now - turnStatusStartedMs;
    if (isPlayer(localRole)) {
        const auto mode = playerDisplayMode(localRole);
        if (mode == scorebot::PlayerDisplayMode::Turn) {
            frame = scorebot::turnStatusFrame(turnElapsed);
        } else if (mode == scorebot::PlayerDisplayMode::WaitingScore &&
                   scorebot::framedScoreScrolls(scoreFor(*this, localRole))) {
            frame = turnElapsed / scorebot::kPlayerScoreScrollMs;
        }
        if (frame != lastTurnStatusFrame) {
            lastTurnStatusFrame = frame;
            if (frame != noAnimation) refreshDisplays(coordinator);
        }
    }

    if (!coordinator->ble.isAuthority()) {
        // A notification can be received while the leader is briefly busy. Keep
        // the original operation id and retry until a newer snapshot commits it.
        if (pendingOperation != 0 && coordinator->ble.hasLeader() &&
            now - pendingSentMs >= kOperationRetryMs && !pendingMessage.isEmpty()) {
            if (coordinator->ble.sendTo(coordinator->ble.authorityNodeId(), pendingMessage)) {
                pendingSentMs = now;
            }
        }
        return;
    }
    if (!gameStarted && now - lastLobbyDisplayMs >= 700) {
        setLeaderboardDisplays(*this, coordinator);
        lastLobbyDisplayMs = now;
    }
    if (!gameStarted && lobbyModeDirty &&
        now - lobbyModeChangedMs >= kLobbyModeSettleMs) {
        commit(*this, coordinator);
    }
    if (now - lastReplicationMs >= kReplicationPeriodMs) {
        leaderId = coordinator->ble.getMyPeerId();
        leaderless = false;
        replicate(*this, coordinator);
        lastReplicationMs = now;
    }
}

void GameState::handleEvent(const Event& event, Coordinator* coordinator) {
#if defined(SCOREBOT_USB_CONTROL)
    if (event.type == EventType::DebugButtonTap) {
        if (event.debugPin == scorebot::kUsbState) {
            Serial.printf("USB STATE %s\n", snapshotJson(*this).c_str());
            return;
        }
        // A synthetic complete tap uses the ordinary release/action path. Never
        // merge it with a held physical key or a maintenance chord.
        const bool idle = !buttonGestureInProgress() &&
            !coordinator->buttonGrid.interruptAsserted() && !coordinator->printerUiActive() &&
            maintenanceAction == scorebot::MaintenanceAction::None;
        if (event.debugPin == scorebot::kUsbPair && idle && coordinator->myRole() != BoardRole::Leader) {
            coordinator->noteInteraction();
            coordinator->ble.chooseColor();
            Serial.println("USB PAIR OPENED");
            return;
        }
        if (event.debugPin < 5 && idle) {
            const ButtonGrid::Interrupt tap{event.debugPin,
                static_cast<uint16_t>(~(1u << event.debugPin)), 0};
            onButtonGridEvent(this, coordinator, &tap);
            Serial.printf("USB DISPATCHED pin=%u draft=%ld game=%lu term=%lu version=%lu\n",
                event.debugPin, static_cast<long>(myScore), static_cast<unsigned long>(gameId),
                static_cast<unsigned long>(term), static_cast<unsigned long>(version));
        } else Serial.println("USB TAP REJECTED physical/maintenance input active");
        return;
    }
#endif
    switch (event.type) {
        case EventType::ButtonPressed:
            onButtonPress(this, event.press, coordinator);
            break;
        case EventType::BleLeaderLost:
            if (!coordinator->ble.isAuthority()) {
                coordinator->observeSessionSleepGrant(false);
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
            JsonDocument envelope;
            if (deserializeJson(envelope, json) == DeserializationError::Ok && envelope["type"] == "pair_busy" &&
                coordinator->ble.acceptsLeader(event.messageReceived.peerId)) {
                coordinator->ble.chooseColor();
            } else if (envelope["type"] == "control") {
                onControllerMessage(this, event, coordinator, envelope);
            } else if (envelope["type"] == "idle") {
                onLeaderSleepGrant(*this, event, coordinator, envelope);
            } else if (PlayerMessage::isPlayerMessage(json)) {
                onPlayerMessage(this, event, coordinator);
            } else if (PlayerActivityMessage::isPlayerActivityMessage(json)) {
                onSessionActivityMessage(this, event, coordinator);
            } else if (PlayerSleepMessage::isPlayerSleepMessage(json)) {
                onPlayerSleepMessage(this, event, coordinator);
            } else if (LeaderResetMessage::isLeaderResetMessage(json)) {
                onLeaderResetMessage(this, event, coordinator);
            } else {
                // State replication is one-way. Never let a player uplink
                // replace authoritative leaderboard state, regardless of the
                // identity embedded in its JSON payload.
                if (!coordinator->ble.acceptsLeader(event.messageReceived.peerId)) {
                    break;
                }
                const bool wasGameStarted = gameStarted;
                const uint32_t previousGameId = gameId;
                const uint32_t previousTerm = term;
                const BoardRole previousTurn = whosTurn;
                const SnapshotDecodeResult decoded = decodeSnapshot(*this, json);
                if (decoded == SnapshotDecodeResult::Invalid ||
                    decoded == SnapshotDecodeResult::Older) {
                    break;
                }
                coordinator->ble.confirmLeader(event.messageReceived.connectionHandle);
                if (gameId != previousGameId || term != previousTerm) {
                    coordinator->observeSessionSleepGrant(false);
                }
                const BoardRole myRole = coordinator->myRole();
                if (isPlayer(myRole)) {
                    const bool acknowledged = scorebot::operationAcknowledged(
                        pendingOperation, operationFor(*this, myRole));
                    const scorebot::PendingDisposition pendingDisposition =
                        scorebot::pendingDisposition(
                            pendingOperation != 0, acknowledged,
                            gameId != previousGameId, term != previousTerm);
                    if (pendingDisposition == scorebot::PendingDisposition::Restore) {
                        // The authoritative epoch changed before this request
                        // was committed. Restore its delta for explicit retry.
                        editPlayerDraft(*this, pendingScore);
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
                if (myRole == BoardRole::Leader) {
                    const size_t slot = scorebot::controllerSlot(controllerNodes, coordinator->ble.getMyPeerId());
                    const uint32_t acknowledged = slot < 4 && controllerNodes[slot] == coordinator->ble.getMyPeerId()
                        ? controllerOperations[slot] : 0;
                    const auto disposition = scorebot::pendingDisposition(pendingOperation != 0,
                        scorebot::operationAcknowledged(pendingOperation, acknowledged),
                        gameId != previousGameId, term != previousTerm);
                    if (disposition == scorebot::PendingDisposition::Restore && isPlayer(pendingControllerRole))
                        addDelta(leaderboardDeltas[roleIndex(pendingControllerRole)], pendingScore);
                    localOperation = scorebot::reconcileOperationId(localOperation, acknowledged);
                    if (disposition != scorebot::PendingDisposition::Keep) {
                        pendingOperation = 0;
                        pendingScore = 0;
                        pendingMessage = "";
                        pendingControllerRole = BoardRole::Unknown;
                    }
                    if (previousTurn != whosTurn || wasGameStarted != gameStarted)
                        selectAuthoritativeTurn(*this, coordinator);
                }
                if (decoded == SnapshotDecodeResult::Equal) {
                    // A rebooted player commonly receives only equal revision
                    // heartbeats. They prove the selected leaderboard is live.
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
