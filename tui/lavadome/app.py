"""Wiring — the game, the renderer, and the scene stack.

Everything that draws lives in :mod:`lavadome.scenes`; everything that decides
lives in :mod:`lavadome.dome`, :mod:`lavadome.betting` and
:mod:`lavadome.handeval`. This module holds them together and owns what
outlives a single run.
"""

from __future__ import annotations

import random

from texastoast.core.tui_game import TuiGame, TuiInput
from texastoast.scene import SceneStack

from lavadome.scenes import GameScene, RulesScene, TitleScene

FPS = 20


class LavaDomeApp:
    """Owns the terminal game and the screen stack."""

    def __init__(self, seed: int | None = None, ascii_only: bool = False,
                 skip_title: bool = False):
        self.rng = random.Random(seed)
        self.seed = seed
        self.ascii_only = ascii_only
        #: Best bank reached, for the life of this process.
        self.best_bank = 0

        # hold_ms=0 gives edge semantics — one keystroke, one action. This is a
        # turn-based card game; a decay timer would turn a single arrow press
        # into a runaway bet adjustment, because terminals report key repeats
        # but never releases.
        self.game = TuiGame(title="Texas Hold'Em Lava Dome",
                            fps=FPS, input_source=TuiInput(hold_ms=0))
        self.renderer = self.game.renderer

        self.stack = SceneStack()
        self.game.set_update(self.update)
        self.game.set_render(self.stack.render)

        # The title screen is always the bottom of the stack, so Esc from a run
        # has somewhere to land and no scene needs an "on title" flag.
        self.stack.push(TitleScene(self))
        if skip_title:
            self.stack.push(GameScene(self))

    # ── Scene transitions ───────────────────────────────────────────

    def start_run(self) -> None:
        self.stack.push(GameScene(self))

    def show_rules(self) -> None:
        self.stack.push(RulesScene(self))

    def pop_scene(self) -> None:
        if len(self.stack) > 1:
            self.stack.pop()

    def record_bank(self, amount: int) -> None:
        self.best_bank = max(self.best_bank, amount)

    # ── Frame ───────────────────────────────────────────────────────

    def update(self, dt: float) -> None:
        """Route keys to the top scene, then run the stack's own update.

        Keys are drained here rather than bound individually because a terminal
        delivers them as a stream and the stack decides who gets them:
        ``dispatch_key`` reaches the top scene only, which is the same modality
        rule that governs updates.
        """
        for key in self.game.input.drain():
            self.stack.dispatch_key(key)
        self.stack.update(dt)

    # ── Introspection, for tests and callers ────────────────────────

    @property
    def scene(self):
        return self.stack.top

    @property
    def in_game(self) -> bool:
        return isinstance(self.stack.top, GameScene)


def run(seed: int | None = None, ascii_only: bool = False,
        skip_title: bool = False) -> None:
    LavaDomeApp(seed, ascii_only, skip_title).game.start()
