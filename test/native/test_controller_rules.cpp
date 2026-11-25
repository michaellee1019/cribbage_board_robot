#include <ControllerRules.hpp>
#include <cassert>
#include <limits>
using namespace scorebot;
int main() {
    Snapshot state{};
    std::array<uint32_t, 4> nodes{}, operations{};
    ControllerAction command{123, 1, ControllerCommand::Lobby, Player::Red, 0, false, LobbyMode::Local};
    assert(applyController(state, nodes, operations, command));
    command.operation++; command.command = ControllerCommand::Start;
    assert(applyController(state, nodes, operations, command));
    assert(state.started && state.rosterMask == 1);
    command.operation++; command.command = ControllerCommand::Score; command.delta = 7; command.passesTurn = true;
    assert(applyController(state, nodes, operations, command));
    assert(state.scores[0] == 7 && state.turn == Player::Red);
    const auto durableState = state;
    const auto durableNodes = nodes, durableOperations = operations;
    assert(!applyController(state, nodes, operations, command));
    assert(state.scores[0] == 7 && state.version == durableState.version);
    // Simulated authority recovery uses the same durable marks.
    state = durableState; nodes = durableNodes; operations = durableOperations;
    assert(!applyController(state, nodes, operations, command));
    command.node = 124; command.operation = 1;
    assert(applyController(state, nodes, operations, command));
    assert(state.scores[0] == 14);
    command.operation++; state.scores[0] = std::numeric_limits<int32_t>::max();
    const uint32_t version = state.version;
    assert(applyController(state, nodes, operations, command)); // Overflow consumed, acknowledged.
    assert(state.scores[0] == std::numeric_limits<int32_t>::max() && state.version == version + 1);
    assert(!applyController(state, nodes, operations, command));
    command.operation = 0;
    assert(!applyController(state, nodes, operations, command));
    command.operation = 3; command.player = Player::None;
    assert(!applyController(state, nodes, operations, command));

    Snapshot orderedLobby{};
    assert(connect(orderedLobby, Player::Red));
    assert(connect(orderedLobby, Player::Blue));
    ControllerAction startCommand{123, 5, ControllerCommand::Start, Player::Red};
    startCommand.turnOrder = kTurnOrderCount;
    assert(!applyController(orderedLobby, nodes, operations, startCommand));
    startCommand.turnOrder = rotatedTurnOrder(kDefaultTurnOrder, 0b0011, 1);
    assert(applyController(orderedLobby, nodes, operations, startCommand));
    assert(orderedLobby.turn == Player::Blue);
    assert(!applyController(orderedLobby, nodes, operations, startCommand));
}
