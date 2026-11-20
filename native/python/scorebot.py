"""Standard-library binding to Scorebot ABI v1. Game policy lives in C++."""

import ctypes as C
from enum import IntEnum
import os
import secrets
from pathlib import Path
import sys
import weakref

ABI_VERSION = 1


class Result(IntEnum):
    OK = 0
    NO_CHANGE = 1
    ACCEPTED_WITHOUT_TURN_CHANGE = 2
    INVALID_ARGUMENT = -1
    ABI_MISMATCH = -2
    NO_MEMORY = -3
    GAME_NOT_STARTED = -4
    NOT_CONNECTED = -5
    DUPLICATE = -6
    SCORE_OUT_OF_RANGE = -7
    REJECTED = -8
    WRONG_GAME = -9
    WRONG_AUTHORITY = -10


class Player(IntEnum):
    RED, BLUE, GREEN, YELLOW, NONE = range(5)
    WHITE = YELLOW  # ABI-v1 source compatibility; slot 3 is Yellow.


class LobbyMode(IntEnum):
    LOCAL, PHYSICAL, OFF = range(3)


class Action(IntEnum):
    ADD, OK = range(2)


class ControllerCommand(IntEnum):
    SCORE, START, LOBBY = range(3)


class PeerKind(IntEnum):
    REJECTED, PLAYER, DISPLAY, CONTROLLER = range(4)


class ScorebotError(RuntimeError):
    def __init__(self, result):
        self.result = Result(result)
        super().__init__(self.result.name)


def _int(value, bits=32, signed=False):
    # ctypes otherwise silently truncates oversized Python integers.
    if not isinstance(value, int):
        raise TypeError("expected an integer")
    low = -(1 << (bits - 1)) if signed else 0
    high = (1 << (bits - int(signed))) - 1
    if not low <= value <= high:
        raise ValueError(f"integer outside {'int' if signed else 'uint'}{bits}")
    return value


def _require(result):
    if result < 0:
        raise ScorebotError(result)
    return Result(result)


class State(C.Structure):
    """Caller-owned snapshot copy; persist to_dict(), never raw struct bytes."""

    _fields_ = [
        ("abi_version", C.c_uint32),
        ("scores", C.c_int32 * 4),
        ("last_operation", C.c_uint32 * 4),
    ] + [(name, C.c_uint32) for name in (
        "game_id", "term", "version", "leader_id", "connected_mask",
        "local_control_mask", "lobby_enabled_mask", "lobby_explicit_mask",
        "sleeping_mask", "roster_mask", "turn", "started",
    )]

    def to_dict(self):
        return {name: list(getattr(self, name)) if name in ("scores", "last_operation")
                else getattr(self, name) for name, _ in self._fields_}

    @classmethod
    def from_dict(cls, values):
        if set(values) != {name for name, _ in cls._fields_}:
            raise ValueError("snapshot must contain exactly the ABI v1 fields")
        state = cls()
        for name, _ in cls._fields_:
            value = values[name]
            if name in ("scores", "last_operation"):
                if len(value) != 4:
                    raise ValueError(f"{name} requires four entries")
                for index, item in enumerate(value):
                    getattr(state, name)[index] = _int(item, signed=name == "scores")
            else:
                setattr(state, name, _int(value))
        return state

    def to_wire(self, protocol, session_id=None):
        """Protocol-v5 state JSON, with this handle's session and controller marks.

        sleeping_mask is a local presence hint and is not part of the wire
        snapshot. Use to_dict() for lossless native persistence instead.
        """
        if session_id is None:
            session_id = getattr(self, "_session_id", None)
        if session_id is None:
            raise ValueError("session_id is required for a standalone State")
        return {"type": "state", "protocol": _int(protocol),
                "game": self.game_id, "session": _int(session_id if session_id is not None else self._session_id),
                "controllers": getattr(self, "_controllers", [0] * 4),
                "controlOps": getattr(self, "_control_ops", [0] * 4),
                "term": self.term, "version": self.version,
                "leader": self.leader_id, "started": bool(self.started),
                "turn": 0 if self.turn == Player.NONE else self.turn + 2,
                "scores": list(self.scores), "operations": list(self.last_operation),
                "connected": self.connected_mask, "local": self.local_control_mask,
                "enabled": self.lobby_enabled_mask, "explicit": self.lobby_explicit_mask,
                "roster": self.roster_mask}


class Library:
    def __init__(self, path=None):
        default = Path(__file__).resolve().parents[1] / "build" / (
            "libscorebot.dylib" if sys.platform == "darwin" else "libscorebot.so")
        self._lib = C.CDLL(str(path or os.environ.get("SCOREBOT_LIBRARY") or default))
        self._lib.scorebot_abi_version.argtypes = []
        self._lib.scorebot_abi_version.restype = C.c_uint32
        if self._lib.scorebot_abi_version() != ABI_VERSION:
            raise ScorebotError(Result.ABI_MISMATCH)
        u, i, p = C.c_uint32, C.c_int32, C.c_void_p
        s, up = C.POINTER(State), C.POINTER(u)
        signatures = {
            "state_size": (u, []), "protocol_version": (u, []), "leader_node_id": (u, []),
            "create": (i, [u, C.POINTER(p)]), "destroy": (None, [p]),
            "read": (i, [p, s, u]), "restore": (i, [p, s, u]),
            "resume_authority": (i, [p]), "set_lobby_mode": (i, [p, u, u]),
            "start": (i, [p]), "reset": (i, [p]),
            "set_connected": (i, [p, u, u]), "mark_sleeping": (i, [p, u]),
            "local_action": (i, [p, u, i, u]),
            "player_action": (i, [p, u, u, u, i, u, u]),
            "admit_peer": (i, [u, u, u, C.c_uint64, u, up, up]),
            "is_leader": (i, [u, u, u, C.c_uint64, u, up]),
            "controller_action": (i, [p, u, u, u, u, i, u, u, u]),
            "controller_marks": (i, [p, up, up, u]),
            "configure_pairing": (i, [p, u, u, u]),
            "pairing_packet": (i, [p, C.POINTER(C.c_uint8), u]),
            "pair_peer": (i, [p, C.POINTER(C.c_uint8), u, C.c_uint64, u, up, up]),
            "compare_revision": (i, [u, u, u, u]),
            "reconcile_operation": (u, [u, u]), "operation_acknowledged": (u, [u, u]),
        }
        for name, (result, args) in signatures.items():
            function = getattr(self._lib, "scorebot_v1_" + name)
            function.restype, function.argtypes = result, args
            setattr(self, "_" + name, function)
        if self._state_size() != C.sizeof(State):
            raise ScorebotError(Result.ABI_MISMATCH)
        self.protocol_version = self._protocol_version()
        self.leader_node_id = self._leader_node_id()

    def game(self, *, node_id=None, session_id=None, pairing_id=None):
        return Game(self, node_id=node_id, session_id=session_id, pairing_id=pairing_id)

    def admit_peer(self, node, protocol, public_address, bluetooth_mac, offset=1):
        kind, player = C.c_uint32(), C.c_uint32()
        _require(self._admit_peer(_int(node), _int(protocol), _int(public_address),
                                 _int(bluetooth_mac, 64), _int(offset), C.byref(kind), C.byref(player)))
        return PeerKind(kind.value), Player(player.value)

    def is_leader(self, node, protocol, public_address, bluetooth_mac, offset=1):
        trusted = C.c_uint32()
        _require(self._is_leader(_int(node), _int(protocol), _int(public_address),
                                _int(bluetooth_mac, 64), _int(offset), C.byref(trusted)))
        return bool(trusted.value)

    def compare_revision(self, incoming_term, incoming_version, current_term, current_version):
        return self._compare_revision(*map(_int, (incoming_term, incoming_version, current_term, current_version)))

    def reconcile_operation(self, local, replicated):
        return self._reconcile_operation(_int(local), _int(replicated))

    def operation_acknowledged(self, pending, replicated):
        return bool(self._operation_acknowledged(_int(pending), _int(replicated)))


class Game:
    """One authority handle. Serialize access externally, including close()."""

    def __init__(self, library, *, node_id=None, session_id=None, pairing_id=None):
        self.library = library
        self._pointer = C.c_void_p()
        _require(library._create(ABI_VERSION, C.byref(self._pointer)))
        self._finalizer = weakref.finalize(self, library._destroy, self._pointer)
        self._pairing = dict(node_id=secrets.randbelow(0xFFFFFFFF) + 1 if node_id is None else _int(node_id),
                            session_id=secrets.randbelow(0xFFFFFFFF) + 1 if session_id is None else _int(session_id),
                            pairing_id=secrets.randbelow(21) if pairing_id is None else _int(pairing_id))
        try:
            _require(self._call("configure_pairing", *self.pairing.values()))
        except Exception:
            self.close()
            raise

    def close(self):
        self._finalizer()

    def __enter__(self):
        self._handle()
        return self

    def __exit__(self, *_):
        self.close()

    def _handle(self):
        if not self._finalizer.alive:
            raise RuntimeError("Scorebot game is closed")
        return self._pointer

    def _call(self, name, *args):
        return Result(getattr(self.library, "_" + name)(self._handle(), *args))

    def read(self):
        state = State()
        _require(self._call("read", C.byref(state), C.sizeof(state)))
        state._session_id = self.pairing["session_id"]
        state._controllers, state._control_ops = self.controller_marks()
        return state

    def restore(self, state):
        if not isinstance(state, State):
            state = State.from_dict(state)
        return self._call("restore", C.byref(state), C.sizeof(state))

    @property
    def pairing(self):
        """Persist with the game snapshot and pass back to Library.game(**pairing)."""
        return dict(self._pairing)

    def pairing_packet(self):
        """Advertise these manufacturer bytes; use them for the pairing GATT value."""
        packet = (C.c_uint8 * 22)()
        _require(self._call("pairing_packet", packet, len(packet)))
        return bytes(packet)

    def admit_peer(self, packet, bluetooth_mac, public_address=True):
        """Bind metadata read on this BLE connection to its transport address."""
        if not isinstance(packet, bytes):
            raise TypeError("pairing packet must be bytes")
        data = (C.c_uint8 * len(packet)).from_buffer_copy(packet)
        kind, player = C.c_uint32(), C.c_uint32()
        _require(self._call("pair_peer", data, len(data), _int(bluetooth_mac, 64),
                            _int(public_address), C.byref(kind), C.byref(player)))
        return PeerKind(kind.value), Player(player.value)

    def controller_action(self, node, operation, command, player, value, passes_turn, game_id, term):
        return self._call("controller_action", _int(node), _int(operation), _int(command), _int(player),
                          _int(value, signed=True), _int(passes_turn), _int(game_id), _int(term))

    def controller_marks(self, restore=None):
        """Persist/restore this pair of four-element arrays alongside the snapshot."""
        nodes, operations = (C.c_uint32 * 4)(), (C.c_uint32 * 4)()
        if restore is not None:
            if len(restore) != 2 or any(len(values) != 4 for values in restore):
                raise ValueError("controller marks require two four-element arrays")
            nodes[:] = [_int(value) for value in restore[0]]
            operations[:] = [_int(value) for value in restore[1]]
        _require(self._call("controller_marks", nodes, operations, int(restore is not None)))
        return list(nodes), list(operations)

    def resume_authority(self):
        return self._call("resume_authority")

    def set_lobby_mode(self, player, mode):
        return self._call("set_lobby_mode", _int(player), _int(mode))

    def start(self):
        return self._call("start")

    def reset(self):
        return self._call("reset")

    def set_connected(self, player, connected):
        return self._call("set_connected", _int(player), _int(connected))

    def mark_sleeping(self, player):
        return self._call("mark_sleeping", _int(player))

    def local_action(self, player, delta, action):
        return self._call("local_action", _int(player), _int(delta, signed=True), _int(action))

    def player_action(self, transport_node, claimed_node, game_id, delta, passes_turn, operation_id):
        return self._call("player_action", _int(transport_node), _int(claimed_node),
                          _int(game_id), _int(delta, signed=True), _int(passes_turn), _int(operation_id))
