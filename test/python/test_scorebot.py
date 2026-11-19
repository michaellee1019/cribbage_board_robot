import ctypes as C
import json
import unittest
from unittest.mock import patch

from scorebot import Action, Library, LobbyMode, PeerKind, Player, Result, ScorebotError, State

RED_NODE = 2934574912
BLUE_NODE = 860931256
RED_MAC = 0x983DAEEA0F41


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

    def mixed_game(self):
        self.assertEqual(self.library.admit_peer(RED_NODE, self.library.protocol_version, True, RED_MAC),
                         (PeerKind.PLAYER, Player.RED))
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
        with self.library.game() as other:
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
        self.assertEqual(self.physical(1), Result.GAME_NOT_STARTED)
        self.mixed_game()
        before = self.game.read().to_dict()
        for arguments, result in (({"claimed_node": BLUE_NODE}, Result.WRONG_AUTHORITY),
                                  ({"transport_node": self.library.leader_node_id}, Result.WRONG_AUTHORITY),
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

    def test_peer_admission_has_no_display_privileges(self):
        library = self.library
        protocol = library.protocol_version
        self.assertEqual(library.admit_peer(RED_NODE, protocol, False, RED_MAC), (PeerKind.REJECTED, Player.NONE))
        self.assertEqual(library.admit_peer(RED_NODE, protocol + 1, True, RED_MAC), (PeerKind.REJECTED, Player.NONE))
        self.assertEqual(library.admit_peer(RED_NODE, protocol, True, RED_MAC ^ (1 << 40)),
                         (PeerKind.REJECTED, Player.NONE))
        self.assertEqual(library.admit_peer(BLUE_NODE, protocol, True, RED_MAC), (PeerKind.REJECTED, Player.NONE))
        mac = 0x112233445566
        node = (mac - 1) & 0xFFFFFFFF
        self.assertEqual(library.admit_peer(node, protocol, True, mac), (PeerKind.DISPLAY, Player.NONE))
        self.assertFalse(library.is_leader(node, protocol, True, mac))
        self.assertEqual(self.physical(1, transport_node=node, claimed_node=node), Result.WRONG_AUTHORITY)
        with self.assertRaises(ScorebotError):
            library.is_leader(node, protocol, True, 2**48)

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
        self.assertEqual((wire["type"], wire["protocol"], wire["turn"], wire["local"]), ("state", 4, 2, 15))
        self.assertNotIn("sleeping_mask", wire)

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
