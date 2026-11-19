#include <scorebot.h>

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static scorebot_state_v1 read_state(scorebot_game* game) {
    scorebot_state_v1 state;
    assert(scorebot_v1_read(game, &state, sizeof(state)) == SCOREBOT_OK);
    return state;
}

int main(void) {
    scorebot_game* game = NULL;
    assert(sizeof(scorebot_state_v1) == 84);
    assert(scorebot_v1_state_size() == sizeof(scorebot_state_v1));
    assert(scorebot_abi_version() == 1);
    assert(scorebot_v1_create(2, &game) == SCOREBOT_ABI_MISMATCH && game == NULL);
    assert(scorebot_v1_create(1, NULL) == SCOREBOT_INVALID_ARGUMENT);
    scorebot_v1_destroy(NULL);
    assert(scorebot_v1_start(NULL) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_reset(NULL) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_resume_authority(NULL) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_set_lobby_mode(NULL, 0, 0) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_set_connected(NULL, 0, 1) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_mark_sleeping(NULL, 0) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_local_action(NULL, 0, 0, 0) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_player_action(NULL, 0, 0, 0, 0, 0, 0) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_create(1, &game) == SCOREBOT_OK && game);
    scorebot_state_v1 state = read_state(game);
    assert(state.turn == SCOREBOT_NO_PLAYER && state.leader_id == scorebot_v1_leader_node_id());
    const scorebot_state_v1 initial = state;
    assert(scorebot_v1_read(NULL, &state, sizeof(state)) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_read(game, NULL, sizeof(state)) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_read(game, &state, sizeof(state) - 1) == SCOREBOT_ABI_MISMATCH);
    assert(memcmp(&state, &initial, sizeof(state)) == 0);
    assert(scorebot_v1_restore(game, NULL, sizeof(state)) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_restore(NULL, &state, sizeof(state)) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_restore(game, &state, sizeof(state) + 4) == SCOREBOT_ABI_MISMATCH);
    state.abi_version++;
    assert(scorebot_v1_restore(game, &state, sizeof(state)) == SCOREBOT_ABI_MISMATCH);
    state = initial;
    state.leader_id ^= 1;
    assert(scorebot_v1_restore(game, &state, sizeof(state)) == SCOREBOT_WRONG_AUTHORITY);
    assert(scorebot_v1_set_lobby_mode(game, 4, 0) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_set_lobby_mode(game, 0, 3) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_local_action(game, 4, 0, 0) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_local_action(game, 0, 0, 2) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_set_connected(game, 0, 2) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_mark_sleeping(game, UINT32_MAX) == SCOREBOT_INVALID_ARGUMENT);
    state = read_state(game);
    assert(memcmp(&state, &initial, sizeof(state)) == 0);
    assert(scorebot_v1_resume_authority(game) == SCOREBOT_OK);
    for (uint32_t player = 0; player < 4; ++player)
        assert(scorebot_v1_set_lobby_mode(game, player, SCOREBOT_LOCAL) == SCOREBOT_OK);
    assert(scorebot_v1_start(game) == SCOREBOT_OK);
    assert(scorebot_v1_local_action(game, 0, 0, SCOREBOT_ADD) == SCOREBOT_NO_CHANGE);
    assert(scorebot_v1_local_action(game, 0, 5, SCOREBOT_ADD) == SCOREBOT_OK);
    assert(scorebot_v1_local_action(game, 0, 0, SCOREBOT_OK_ACTION) == SCOREBOT_OK);
    state = read_state(game);
    assert(state.connected_mask == 0 && state.roster_mask == 15);
    assert(state.scores[0] == 5 && state.turn == 1 && state.last_operation[0] == 0);

    uint32_t kind = 999, player = 999, trusted = 999;
    assert(scorebot_v1_admit_peer(0, 4, 1, 0, 0, &kind, &player) == SCOREBOT_INVALID_ARGUMENT);
    assert(kind == 999 && player == 999);
    assert(scorebot_v1_admit_peer(0, 4, 1, 0, 1, &kind, &kind) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_is_leader(0, 4, 2, 0, 1, &trusted) == SCOREBOT_INVALID_ARGUMENT && trusted == 999);
    assert(scorebot_v1_is_leader(0, 4, 1, 0, 1, NULL) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_admit_peer(0, 4, 1, 0, 1, NULL, &player) == SCOREBOT_INVALID_ARGUMENT);
    assert(scorebot_v1_admit_peer(0, 4, 1, 0, 1, &kind, NULL) == SCOREBOT_INVALID_ARGUMENT);
    scorebot_v1_destroy(game);
    puts("C ABI ownership and validation tests passed");
    return 0;
}
