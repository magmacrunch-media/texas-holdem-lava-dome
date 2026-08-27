"""Wiring — what outlives a single run, and how the screens reach it.

Everything that draws lives in :mod:`lavadome.scenes`; everything that decides
lives in :mod:`lavadome.dome`, :mod:`lavadome.betting` and
:mod:`lavadome.handeval`.

**The terminal is not owned here.** It belongs to a
:class:`~texastoast.core.tui_host.TuiHost`, which this is handed. That is what
lets the game run as its own command *and* be seated by a launcher without
knowing which happened.

What is left is genuinely this game's: the shuffle, whether suits are drawn as
glyphs, and the best bank reached.
"""

from __future__ import annotations

import random
from typing import Any

from lavadome.scenes import GameScene, RulesScene, TitleScene


class LavaDomeApp:
    """A session of Lava Dome, drawing on somebody else's terminal."""

    def __init__(self, host: Any, seed: int | None = None,
                 ascii_only: bool = False):
        self.host = host
        self.rng = random.Random(seed)
        self.seed = seed
        self.ascii_only = ascii_only
        #: Best bank reached, for as long as this session lasts.
        self.best_bank = 0

        #: The title screen. The caller pushes it — a game that pushed its own
        #: scene would take that decision away from whatever is seating it.
        self.root_scene = TitleScene(self)

    # ── What the scenes reach for ───────────────────────────────────

    @property
    def renderer(self):
        return self.host.renderer

    @property
    def game(self):
        """The terminal app.

        Named ``game`` because that is what the scenes called it when this
        class owned one. It is the host's now.
        """
        return self.host

    def start_run(self) -> None:
        self.host.push_scene(GameScene(self))

    def show_rules(self) -> None:
        self.host.push_scene(RulesScene(self))

    def pop_scene(self) -> None:
        """Leave the top screen.

        Popping the title screen ends a standalone session and returns to the
        arcade menu under a launcher. The game does not need to know which —
        see ``TuiHost.pop_scene``.
        """
        self.host.pop_scene()

    def record_bank(self, amount: int) -> None:
        self.best_bank = max(self.best_bank, amount)

    # ── Introspection, for tests ────────────────────────────────────

    @property
    def scene(self):
        return self.host.scene

    @property
    def in_game(self) -> bool:
        return isinstance(self.host.scene, GameScene)


def run(seed: int | None = None, ascii_only: bool = False,
        skip_title: bool = False) -> None:
    """Play Lava Dome as its own command."""
    from texastoast.core.tui_host import TuiHost

    from lavadome.arcade import GAME

    host = TuiHost(title=GAME.info.title, fps=GAME.info.fps,
                   hold_ms=GAME.info.hold_ms)
    app = LavaDomeApp(host, seed, ascii_only)
    host.push_scene(app.root_scene)
    if skip_title:
        host.push_scene(GameScene(app))
    host.run()
