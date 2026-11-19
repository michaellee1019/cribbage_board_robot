"""Run with PYTHONPATH=native/python python3 native/examples/local_game.py."""
import json

from scorebot import Action, Library, LobbyMode, Player, Result

library = Library()  # Or Library("/absolute/path/to/locally-built/libscorebot.so")
with library.game() as game:
    assert game.resume_authority() == Result.OK
    for player in (Player.RED, Player.BLUE, Player.GREEN, Player.WHITE):
        assert game.set_lobby_mode(player, LobbyMode.LOCAL) == Result.OK
    assert game.start() == Result.OK
    assert game.local_action(Player.RED, 3, Action.ADD) == Result.OK
    assert game.local_action(Player.RED, 0, Action.OK) == Result.OK
    saved = json.dumps(game.read().to_dict())  # Caller owns durable I/O.
    with library.game(**game.pairing) as restored:
        assert restored.restore(json.loads(saved)) == Result.OK
        assert restored.read().to_dict() == game.read().to_dict()
    print(json.dumps(game.read().to_wire(library.protocol_version), indent=2))
