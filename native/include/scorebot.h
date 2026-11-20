#ifndef SCOREBOT_C_API_H
#define SCOREBOT_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCOREBOT_API __attribute__((visibility("default")))
#define SCOREBOT_ABI_VERSION 1u
#define SCOREBOT_PLAYER_COUNT 4u

typedef struct scorebot_game scorebot_game;
typedef int32_t scorebot_result;
enum {
    SCOREBOT_OK = 0,
    SCOREBOT_NO_CHANGE = 1,
    SCOREBOT_ACCEPTED_WITHOUT_TURN_CHANGE = 2,
    SCOREBOT_INVALID_ARGUMENT = -1,
    SCOREBOT_ABI_MISMATCH = -2,
    SCOREBOT_NO_MEMORY = -3,
    SCOREBOT_GAME_NOT_STARTED = -4,
    SCOREBOT_NOT_CONNECTED = -5,
    SCOREBOT_DUPLICATE = -6,
    SCOREBOT_SCORE_OUT_OF_RANGE = -7,
    SCOREBOT_REJECTED = -8,
    SCOREBOT_WRONG_GAME = -9,
    SCOREBOT_WRONG_AUTHORITY = -10
};
enum { SCOREBOT_RED = 0, SCOREBOT_BLUE = 1, SCOREBOT_GREEN = 2,
       SCOREBOT_YELLOW = 3, SCOREBOT_WHITE = SCOREBOT_YELLOW, SCOREBOT_NO_PLAYER = 4 };
enum { SCOREBOT_LOCAL = 0, SCOREBOT_PHYSICAL = 1, SCOREBOT_OFF = 2 };
enum { SCOREBOT_ADD = 0, SCOREBOT_OK_ACTION = 1 };
enum { SCOREBOT_PEER_REJECTED = 0, SCOREBOT_PEER_PLAYER = 1, SCOREBOT_PEER_DISPLAY = 2, SCOREBOT_PEER_CONTROLLER = 3 };

// A COPY, never a C++ layout or an on-disk/wire binary format. All fields are
// 32-bit; array order is red/blue/green/yellow. Booleans are exactly 0 or 1,
// masks use bits 0..3, turn is 0..4. No packing directives are needed.
typedef struct scorebot_state_v1 {
    uint32_t abi_version;
    int32_t scores[4];
    uint32_t last_operation[4];
    uint32_t game_id;
    uint32_t term;
    uint32_t version;
    uint32_t leader_id;
    uint32_t connected_mask;
    uint32_t local_control_mask;
    uint32_t lobby_enabled_mask;
    uint32_t lobby_explicit_mask;
    uint32_t sleeping_mask;
    uint32_t roster_mask;
    uint32_t turn;
    uint32_t started;
} scorebot_state_v1;

// Ownership: create initializes *out to NULL on failure (if out is valid).
// Destroy exactly once; destroy(NULL) is harmless. Other NULL pointers return
// INVALID_ARGUMENT. Non-NULL pointers must address valid objects/buffers of the
// indicated size. A handle is single-threaded: externally serialize all calls,
// including reads and destruction. No borrowed pointers or exceptions escape.
// Invalid inputs and ABI errors leave state and output buffers unchanged.
// Handles cannot be used after destruction; arbitrary pointers are not checked.
SCOREBOT_API uint32_t scorebot_abi_version(void);
SCOREBOT_API uint32_t scorebot_v1_state_size(void);
SCOREBOT_API uint32_t scorebot_v1_protocol_version(void);
SCOREBOT_API uint32_t scorebot_v1_leader_node_id(void);
SCOREBOT_API scorebot_result scorebot_v1_create(uint32_t abi_version, scorebot_game** out);
SCOREBOT_API void scorebot_v1_destroy(scorebot_game* game);
SCOREBOT_API scorebot_result scorebot_v1_read(const scorebot_game* game, scorebot_state_v1* out, uint32_t size);
// Restore trusted local persistence, never an untrusted network snapshot.
// Requires exact ABI/size, configured leader and a valid snapshot. Replaces
// state atomically, including all operation high-water marks and metadata.
SCOREBOT_API scorebot_result scorebot_v1_restore(scorebot_game* game, const scorebot_state_v1* state, uint32_t size);
// Call after create/restore, persist before publishing: drops live links,
// prunes implicit lobby availability, increments the authority term.
SCOREBOT_API scorebot_result scorebot_v1_resume_authority(scorebot_game* game);
SCOREBOT_API scorebot_result scorebot_v1_set_lobby_mode(scorebot_game* game, uint32_t player, uint32_t mode);
SCOREBOT_API scorebot_result scorebot_v1_start(scorebot_game* game);
// Ordinary new lobby: preserves profile, game id and operation high-water marks.
// This is not the firmware's destructive maintenance/factory-reset chord.
SCOREBOT_API scorebot_result scorebot_v1_reset(scorebot_game* game);
SCOREBOT_API scorebot_result scorebot_v1_set_connected(scorebot_game* game, uint32_t player, uint32_t connected);
// Sleep is a presence hint, without a revision increment; disconnect separately.
SCOREBOT_API scorebot_result scorebot_v1_mark_sleeping(scorebot_game* game, uint32_t player);
// Delta is the caller's edit buffer. Uses the firmware ADD/OK rules: ADD(0)
// is NO_CHANGE, OK(0) can pass. On a SCOREBOT_OK result the caller clears its
// edit buffer after either ADD or OK; rejection leaves the edit buffer intact.
SCOREBOT_API scorebot_result scorebot_v1_local_action(scorebot_game* game, uint32_t player, int32_t delta, uint32_t action);
// transport_node must come from an admitted transport, not from the payload.
// Valid physical overflow CONSUMES the operation and increments version, just
// like firmware; duplicates do not. Local overflow returns REJECTED unchanged.
SCOREBOT_API scorebot_result scorebot_v1_player_action(scorebot_game* game, uint32_t transport_node,
    uint32_t claimed_node, uint32_t game_id, int32_t delta, uint32_t passes_turn, uint32_t operation_id);

// Deprecated process-global queries: validated arguments return rejected/false.
// Use the per-game runtime pairing API below. Retained for ABI-v1 compatibility.
SCOREBOT_API scorebot_result scorebot_v1_admit_peer(uint32_t node, uint32_t protocol, uint32_t public_address,
    uint64_t bluetooth_mac, uint32_t offset, uint32_t* out_kind, uint32_t* out_player);
SCOREBOT_API scorebot_result scorebot_v1_is_leader(uint32_t node, uint32_t protocol, uint32_t public_address,
    uint64_t bluetooth_mac, uint32_t offset, uint32_t* out_trusted);
// Runtime authority configuration. Configure immediately after create; persist
// these three values alongside the v1 game snapshot and reuse them on recovery.
// Legacy process-global leader/admission queries fail closed in protocol v5.
#define SCOREBOT_DISCOVERY_SIZE 22u
SCOREBOT_API scorebot_result scorebot_v1_configure_pairing(scorebot_game* game,
    uint32_t node, uint32_t session, uint32_t pairing_id);
SCOREBOT_API scorebot_result scorebot_v1_pairing_packet(const scorebot_game* game, uint8_t* out, uint32_t size);
// packet is the live pairing characteristic, read on this transport connection.
// public_address is 1 for public, 0 for random; both are supported at runtime.
// Wrong sessions, occupied colors, and incompatible packets return REJECTED kind.
SCOREBOT_API scorebot_result scorebot_v1_pair_peer(scorebot_game* game, const uint8_t* packet, uint32_t size,
    uint64_t bluetooth_address, uint32_t public_address, uint32_t* out_kind, uint32_t* out_player);

// Mirrored-controller commands: 0=score, 1=start, 2=lobby (value is lobby mode).
// node is the admitted transport identity. Persist marks with the game snapshot.
SCOREBOT_API scorebot_result scorebot_v1_controller_action(scorebot_game* game, uint32_t node, uint32_t operation,
    uint32_t command, uint32_t player, int32_t value, uint32_t passes_turn, uint32_t game_id, uint32_t term);
// Each buffer has four uint32 entries. restore=0 reads; restore=1 restores
// trusted local persistence, atomically after validating all eight entries.
SCOREBOT_API scorebot_result scorebot_v1_controller_marks(scorebot_game* game, uint32_t* nodes,
    uint32_t* operations, uint32_t restore);

// Ordering result -1/0/1; operation helpers use existing non-wrapping high-water semantics.
SCOREBOT_API int32_t scorebot_v1_compare_revision(uint32_t incoming_term, uint32_t incoming_version,
    uint32_t current_term, uint32_t current_version);
SCOREBOT_API uint32_t scorebot_v1_reconcile_operation(uint32_t local, uint32_t replicated);
SCOREBOT_API uint32_t scorebot_v1_operation_acknowledged(uint32_t pending, uint32_t replicated);

#ifdef __cplusplus
}
#endif
#endif
