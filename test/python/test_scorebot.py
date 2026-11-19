import ctypes as C
import json
import struct
import unittest
from unittest.mock import patch

from scorebot import Action, ControllerCommand, Library, LobbyMode, PeerKind, Player, Result, ScorebotError, State

RED_NODE = 0xabcdef01
BLUE_NODE = 0xabcdef02
RED_MAC = 0x112233445566


class ScorebotTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = Library()

    def setUp(self):
        self.game = self.library.game()
        self.addCleanup(self.game.close)

    def local_game(self):
        self.assertEqual(self.game.resume_authority(), Result.OK)
        for player in range(4):
            self.assertEqual(self.game.set_lobby_mode(player, LobbyMode.LOCAL), Result.OK)
        self.assertEqual(self.game.start(), Result.OK)

    def peer_packet(self, node=RED_NODE, role=2, kind=1, **overrides):
        config = self.game.pairing
        fields = dict(node=node, leader=config["node_id"], session=config["session_id"],
                      pairing=config["pairing_id"], kind=kind, role=role, flags=2, occupied=0)
        fields.update(overrides)
        return struct.pack("<5sIIIBBBBB", b"\xff\xffSP" + bytes([self.library.protocol_version]), *fields.values())

    def admit_red(self):
        self.assertEqual(self.game.admit_peer(self.peer_packet(), RED_MAC), (PeerKind.PLAYER, Player.RED))

    def mixed_game(self):
        self.admit_red()
        self.assertEqual(self.game.set_connected(Player.RED, True), Result.OK)
        self.assertEqual(self.game.set_lobby_mode(Player.BLUE, LobbyMode.LOCAL), Result.OK)
        self.assertEqual(self.game.start(), Result.OK)

    def physical(self, delta=0, passes=False, operation=1, **overrides):
        args = dict(transport_node=RED_NODE, claimed_node=RED_NODE, game_id=self.game.read().game_id,
                    delta=delta, passes_turn=passes, operation_id=operation)
        args.update(overrides)
        return self.game.player_action(**args)

    def test_all_local_add_ok_corrections_and_wrap(self):
        self.local_game()
        state = self.game.read()
        self.assertEqual((state.connected_mask, state.roster_mask, state.turn), (0, 15, Player.RED))
        self.assertEqual(self.game.local_action(Player.RED, 0, Action.ADD), Result.NO_CHANGE)
        self.assertEqual(state.to_dict(), self.game.read().to_dict())
        self.assertEqual(self.game.local_action(Player.RED, 3, Action.ADD), Result.OK)
        self.assertEqual(self.game.read().turn, Player.RED)
        self.assertEqual(self.game.local_action(Player.BLUE, -2, Action.OK), Result.OK)
        self.assertEqual(self.game.read().turn, Player.RED)  # Out-of-turn correction.
        for player in range(4):
            self.assertEqual(self.game.local_action(player, 0, Action.OK), Result.OK)
        state = self.game.read()
        self.assertEqual(state.turn, Player.RED)
        self.assertEqual(list(state.scores), [3, -2, 0, 0])
        self.assertEqual(list(state.last_operation), [0, 0, 0, 0])
        self.assertEqual(state.version, 11)

    def test_mixed_players_duplicates_and_rescue(self):
        self.mixed_game()
        self.assertEqual(self.physical(5, True, 7), Result.OK)
        self.assertEqual(self.game.read().turn, Player.BLUE)
        before = self.game.read().to_dict()
        self.assertEqual(self.physical(5, True, 7), Result.DUPLICATE)
        self.assertEqual(self.physical(5, True, 6), Result.DUPLICATE)
        self.assertEqual(before, self.game.read().to_dict())
        self.assertEqual(self.physical(-2, True, 8), Result.ACCEPTED_WITHOUT_TURN_CHANGE)
        self.assertEqual(self.game.read().turn, Player.BLUE)
        self.assertEqual(self.game.set_connected(Player.RED, False), Result.OK)
        self.assertEqual(self.physical(1, False, 9), Result.NOT_CONNECTED)
        self.assertEqual(self.game.local_action(Player.RED, 2, Action.ADD), Result.OK)
        self.assertEqual(self.game.read().scores[0], 5)
        self.assertEqual(self.game.read().last_operation[0], 8)
        self.assertEqual(self.game.set_connected(Player.WHITE, True), Result.NO_CHANGE)
        self.assertEqual(self.game.set_lobby_mode(Player.GREEN, LobbyMode.LOCAL), Result.NO_CHANGE)

    def test_sleep_and_reconnect_keep_frozen_order(self):
        self.mixed_game()
        version = self.game.read().version
        self.assertEqual(self.game.mark_sleeping(Player.RED), Result.OK)
        self.assertEqual(self.game.mark_sleeping(Player.RED), Result.NO_CHANGE)
        self.assertEqual(self.game.read().version, version)
        self.assertEqual(self.game.set_connected(Player.RED, False), Result.OK)
        self.assertEqual(self.game.local_action(Player.RED, 0, Action.OK), Result.OK)
        self.assertEqual(self.game.read().turn, Player.BLUE)
        self.assertEqual(self.game.local_action(Player.BLUE, 0, Action.OK), Result.OK)
        self.assertEqual(self.game.read().turn, Player.RED)
        self.assertEqual(self.game.set_connected(Player.RED, True), Result.OK)
        self.assertEqual(self.game.read().sleeping_mask, 0)

    def test_signed_overflow_consumption_differs_for_local_and_physical(self):
        self.mixed_game()
        for extreme, delta in ((2**31 - 1, 1), (-2**31, -1)):
            state = self.game.read().to_dict()
            state["scores"][0] = extreme
            self.assertEqual(self.game.restore(state), Result.OK)
            before = self.game.read().to_dict()
            self.assertEqual(self.game.local_action(Player.RED, delta, Action.OK), Result.REJECTED)
            self.assertEqual(self.game.read().to_dict(), before)
            op = before["last_operation"][0] + 1
            self.assertEqual(self.physical(delta, True, op), Result.SCORE_OUT_OF_RANGE)
            after = self.game.read()
            self.assertEqual((after.scores[0], after.turn), (extreme, Player.RED))
            self.assertEqual(after.last_operation[0], op)
            self.assertEqual(after.version, before["version"] + 1)
            self.assertEqual(self.physical(delta, True, op), Result.DUPLICATE)

    def test_roundtrip_reset_high_water_and_resume(self):
        self.mixed_game()
        state = self.game.read().to_dict()
        state["game_id"], state["term"] = 42, 19
        self.assertEqual(self.game.restore(state), Result.OK)
        self.assertEqual(self.physical(8, False, 0xFFFFFFFF), Result.OK)
        self.assertEqual(self.game.mark_sleeping(Player.RED), Result.OK)
        saved = json.loads(json.dumps(self.game.read().to_dict()))
        with self.library.game(**self.game.pairing) as other:
            self.assertEqual(other.restore(saved), Result.OK)
            self.assertEqual(other.read().to_dict(), saved)
            self.assertEqual(other.resume_authority(), Result.OK)
            resumed = other.read()
            self.assertEqual((resumed.game_id, resumed.term, resumed.version), (42, 20, saved["version"]))
            self.assertEqual((resumed.connected_mask, resumed.roster_mask, resumed.sleeping_mask), (0, 3, 1))
            self.assertEqual(resumed.last_operation[0], 0xFFFFFFFF)
        self.assertEqual(self.game.reset(), Result.OK)
        reset = self.game.read()
        self.assertEqual(reset.last_operation[0], 0xFFFFFFFF)
        self.assertEqual((reset.game_id, reset.term, reset.roster_mask, reset.turn), (42, 19, 0, Player.NONE))
        self.assertEqual(list(reset.scores), [0] * 4)
        self.assertEqual(self.game.start(), Result.OK)
        self.assertEqual(self.physical(8, False, 0xFFFFFFFF), Result.DUPLICATE)

    def test_resume_prunes_implicit_lobby_only(self):
        self.game.set_connected(Player.RED, True)
        self.game.set_lobby_mode(Player.BLUE, LobbyMode.LOCAL)
        self.game.set_lobby_mode(Player.GREEN, LobbyMode.PHYSICAL)
        self.assertEqual(self.game.resume_authority(), Result.OK)
        state = self.game.read()
        self.assertEqual((state.connected_mask, state.lobby_enabled_mask, state.local_control_mask), (0, 6, 2))
        state.term = 0xFFFFFFFF
        self.assertEqual(self.game.restore(state), Result.OK)
        self.assertEqual(self.game.resume_authority(), Result.REJECTED)

    def test_invalid_snapshot_is_atomic(self):
        self.local_game()
        original = self.game.read().to_dict()
        invalid = [("abi_version", 2, Result.ABI_MISMATCH),
                   ("leader_id", 0, Result.WRONG_AUTHORITY)]
        invalid += [(key, value, Result.INVALID_ARGUMENT) for key, value in (
            ("connected_mask", 16), ("local_control_mask", 16), ("lobby_explicit_mask", 16),
            ("sleeping_mask", 16), ("started", 2), ("turn", 5), ("turn", 4),
            ("roster_mask", 0), ("lobby_enabled_mask", 0), ("started", 0))]
        for key, value, result in invalid:
            with self.subTest(key=key, value=value):
                changed = dict(original, **{key: value})
                self.assertEqual(self.game.restore(changed), result)
                self.assertEqual(self.game.read().to_dict(), original)

    def test_invalid_actions_and_physical_authority(self):
        self.assertEqual(self.game.local_action(Player.RED, 1, Action.ADD), Result.REJECTED)
        self.admit_red()
        self.assertEqual(self.physical(1), Result.GAME_NOT_STARTED)
        self.mixed_game()
        before = self.game.read().to_dict()
        for arguments, result in (({"claimed_node": BLUE_NODE}, Result.WRONG_AUTHORITY),
                                  ({"transport_node": self.game.pairing["node_id"]}, Result.WRONG_AUTHORITY),
                                  ({"transport_node": 123, "claimed_node": 123}, Result.WRONG_AUTHORITY),
                                  ({"game_id": 99}, Result.WRONG_GAME),
                                  ({"operation_id": 0}, Result.INVALID_ARGUMENT),
                                  ({"passes_turn": 2}, Result.INVALID_ARGUMENT)):
            self.assertEqual(self.physical(1, **arguments), result)
            self.assertEqual(self.game.read().to_dict(), before)
        self.assertEqual(self.game.local_action(Player.NONE, 0, Action.ADD), Result.INVALID_ARGUMENT)
        for value in (-1, 2**32):
            with self.assertRaises(ValueError):
                self.game.set_connected(value, True)
        with self.assertRaises(ValueError):
            self.game.local_action(Player.RED, 2**31, Action.ADD)
        with self.assertRaises(ValueError):
            State.from_dict(dict(before, scores=[0, 0]))
        with self.assertRaises(ValueError):
            State.from_dict(dict(before, last_operation=[-1] * 4))

    def test_runtime_pairing_isolated_and_color_bound(self):
        packet = self.peer_packet()
        self.assertEqual(self.game.admit_peer(packet, RED_MAC, False), (PeerKind.PLAYER, Player.RED))
        for changed in (self.peer_packet(session=1), self.peer_packet(leader=1), self.peer_packet(flags=0),
                        self.peer_packet(role=0), packet[:4] + b"\x04" + packet[5:], packet[:-1]):
            self.assertEqual(self.game.admit_peer(changed, RED_MAC), (PeerKind.REJECTED, Player.NONE))
        self.assertEqual(self.game.admit_peer(packet, RED_MAC ^ (1 << 40), False),
                         (PeerKind.REJECTED, Player.NONE))
        self.assertEqual(self.game.admit_peer(self.peer_packet(node=BLUE_NODE), RED_MAC + 1),
                         (PeerKind.REJECTED, Player.NONE))
        display = self.peer_packet(node=99, role=0, kind=3)
        self.assertEqual(self.game.admit_peer(display, RED_MAC + 2), (PeerKind.DISPLAY, Player.NONE))
        self.assertEqual(self.physical(1, transport_node=99, claimed_node=99), Result.WRONG_AUTHORITY)
        with self.library.game(**self.game.pairing) as independent:
            self.assertEqual(independent.admit_peer(self.peer_packet(node=BLUE_NODE), RED_MAC + 3),
                             (PeerKind.PLAYER, Player.RED))
        with self.library.game() as unrelated:
            self.assertEqual(unrelated.admit_peer(packet, RED_MAC), (PeerKind.REJECTED, Player.NONE))
        # Legacy admission cannot infer trust from a factory address.
        self.assertEqual(self.library.admit_peer(RED_NODE, self.library.protocol_version, True, RED_MAC),
                         (PeerKind.REJECTED, Player.NONE))
        self.assertFalse(self.library.is_leader(RED_NODE, self.library.protocol_version, True, RED_MAC))
        with self.assertRaises(ScorebotError):
            self.game.admit_peer(packet, 2**48)

    def test_mirrored_controller_commands_and_recovery(self):
        self.local_game()
        controller = self.peer_packet(node=0x77889900, role=1, kind=2)
        self.assertEqual(self.game.admit_peer(controller, RED_MAC + 8), (PeerKind.CONTROLLER, Player.NONE))
        state = self.game.read()
        args = (0x77889900, 1, ControllerCommand.SCORE, Player.RED, 7, False, state.game_id, state.term)
        self.assertEqual(self.game.controller_action(*args), Result.OK)
        self.assertEqual(self.game.controller_action(*args), Result.DUPLICATE)
        self.assertEqual(self.game.read().scores[0], 7)
        marks = self.game.controller_marks()
        wire = self.game.read().to_wire(self.library.protocol_version)
        self.assertEqual((wire["controllers"][0], wire["controlOps"][0]), (0x77889900, 1))
        with self.library.game(**self.game.pairing) as other:
            self.assertEqual(other.restore(self.game.read().to_dict()), Result.OK)
            other.controller_marks(marks)
            other.admit_peer(controller, RED_MAC + 8)
            self.assertEqual(other.controller_action(*args), Result.DUPLICATE)
            self.assertEqual(other.read().scores[0], 7)
            with self.assertRaises(ScorebotError):
                other.controller_marks(([1, 1, 0, 0], [2, 2, 0, 0]))
            self.assertEqual(other.controller_marks(), marks)

    def test_pairing_defaults_bounds_and_packet(self):
        self.assertTrue(0 <= self.game.pairing["pairing_id"] <= 20)
        self.assertEqual(len(self.game.pairing_packet()), 22)
        for pairing in (0, 20):
            with self.library.game(node_id=123, session_id=456, pairing_id=pairing) as game:
                self.assertEqual(game.read().leader_id, 123)
                self.assertEqual(game.pairing_packet()[17], pairing)
        for config in ({"node_id": 0}, {"session_id": 0}, {"pairing_id": 21}):
            with self.assertRaises(ScorebotError):
                self.library.game(**config)
        self.local_game()
        # Changing authority after gameplay starts is refused atomically.
        before = self.game.read().to_dict()
        self.assertEqual(self.game._call("configure_pairing", 1, 2, 3), Result.REJECTED)
        self.assertEqual(self.game.read().to_dict(), before)

    def test_revision_operation_helpers_and_wire_shape(self):
        library = self.library
        self.assertEqual(library.compare_revision(2, 0, 1, 0xFFFFFFFF), 1)
        self.assertEqual(library.compare_revision(1, 3, 1, 3), 0)
        self.assertEqual(library.compare_revision(1, 3, 1, 4), -1)
        self.assertEqual(library.reconcile_operation(3, 7), 7)
        self.assertEqual(library.reconcile_operation(9, 7), 9)
        self.assertFalse(library.operation_acknowledged(0, 10))
        self.assertFalse(library.operation_acknowledged(11, 10))
        self.assertTrue(library.operation_acknowledged(10, 10))
        self.local_game()
        wire = self.game.read().to_wire(library.protocol_version)
        self.assertEqual((wire["type"], wire["protocol"], wire["turn"], wire["local"]), ("state", 5, 2, 15))
        self.assertNotIn("sleeping_mask", wire)
        self.assertEqual(wire["session"], self.game.pairing["session_id"])

    def test_widest_wire_snapshot_fits_att_attribute(self):
        state = self.game.read()
        for index in range(4):
            state.scores[index] = -2**31
            state.last_operation[index] = 2**32 - 1
        state._controllers = [2**32 - 1 - index for index in range(4)]
        state._control_ops = [2**32 - 1] * 4
        state._session_id = state.leader_id = state.game_id = state.term = state.version = 2**32 - 1
        for field in ("connected_mask", "local_control_mask", "lobby_enabled_mask", "lobby_explicit_mask", "roster_mask"):
            setattr(state, field, 15)
        wire = json.dumps(state.to_wire(self.library.protocol_version), separators=(",", ":"))
        self.assertLessEqual(len(wire.encode()), 512)

    def test_binding_versions_ownership_and_copy(self):
        with patch("scorebot.C.CDLL") as loader:
            loader.return_value.scorebot_abi_version.return_value = 99
            with self.assertRaises(ScorebotError):
                Library("unused")
        with patch("scorebot.C.CDLL") as loader:
            loader.return_value.scorebot_abi_version.return_value = 1
            loader.return_value.scorebot_v1_state_size.return_value = 999
            with self.assertRaises(ScorebotError):
                Library("unused")
        copy = self.game.read()
        copy.scores[0] = 99
        self.assertEqual(self.game.read().scores[0], 0)
        self.game.close()
        self.game.close()
        with self.assertRaises(RuntimeError):
            self.game.read()


if __name__ == "__main__":
    unittest.main()
