#pragma once
#include <GameRules.hpp>
#include <array>

namespace scorebot {
enum class ControllerCommand : uint8_t { Score, Start, Lobby };
struct ControllerAction {
    uint32_t node{0};
    uint32_t operation{0};
    ControllerCommand command{ControllerCommand::Score};
    Player player{Player::None};
    int32_t delta{0};
    bool passesTurn{false};
    LobbyMode mode{LobbyMode::Off};
    uint8_t turnOrder{kDefaultTurnOrder};
};
inline size_t controllerSlot(const std::array<uint32_t, 4>& nodes, uint32_t node) {
    size_t empty = nodes.size();
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i] == node && node != 0) return i;
        if (nodes[i] == 0 && empty == nodes.size()) empty = i;
    }
    return empty;
}
// Authority and session validation happen before this helper. A consumed
// controller command is acknowledged in the same durable snapshot as its
// effect, so retries cannot apply it twice even across authority restarts.
inline bool applyController(Snapshot& state, std::array<uint32_t, 4>& nodes,
                            std::array<uint32_t, 4>& operations, const ControllerAction& action) {
    if (action.node == 0 || action.operation == 0 || !isPlayer(action.player) ||
        action.command > ControllerCommand::Lobby || action.mode > LobbyMode::Off ||
        (action.command == ControllerCommand::Start && !isTurnOrder(action.turnOrder))) return false;
    const size_t slot = controllerSlot(nodes, action.node);
    if (slot == nodes.size() || (nodes[slot] == action.node && action.operation <= operations[slot])) return false;
    const uint32_t previousVersion = state.version;
    if (action.command == ControllerCommand::Start) start(state, action.turnOrder);
    else if (action.command == ControllerCommand::Lobby) setLobbyMode(state, action.player, action.mode);
    else applyLocal(state, {action.player, action.delta, action.passesTurn});
    nodes[slot] = action.node;
    operations[slot] = action.operation;
    if (state.version == previousVersion) ++state.version;
    return true;
}
}
