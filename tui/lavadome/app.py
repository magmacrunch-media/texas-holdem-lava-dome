"""Wiring — what outlives a single run, and how the screens reach it.

Everything that draws lives in :mod:`lavadome.scenes`; everything that decides
lives in :mod:`lavadome.dome`, :mod:`lavadome.betting` and
:mod:`lavadome.handeval`.

**The terminal is not owned here.** It belongs to a
:class:`~magmacrunch.engine.core.tui_host.TuiHost`, which this is handed. That is what
lets the game run as its own command *and* be seated by a launcher without
knowing which happened.

What is left is genuinely this game's: the shuffle, whether suits are drawn as
glyphs, and the best bank reached.
"""

from __future__ import annotations

import random
from typing import Any

from magmacrunch.engine.scores import ScoreBook

from lavadome.arcade import SCORE_KEY as ARCADE_SCORE_KEY
from lavadome.scenes import GameScene, RulesScene, TitleScene


class LavaDomeApp:
    """A session of Lava Dome, drawing on somebody else's terminal."""

    #: The key the browser build posts under. Not "thld" — the web has filed
    #: this under solitaire-thld since before the rename, and a shared board
    #: later has to mean a shared board rather than two with different names.
    #:
    #: Read from :mod:`lavadome.arcade` rather than written out again here.
    #: The arcade menu needs the same string to draw this cabinet's best score
    #: without importing any of this, so that module owns it — and one literal
    #: cannot disagree with itself.
    SCORE_KEY = ARCADE_SCORE_KEY

    def __init__(self, host: Any, seed: int | None = None,
                 ascii_only: bool = False, scores: ScoreBook | None = None):
        self.host = host
        self.rng = random.Random(seed)
        self.seed = seed
        self.ascii_only = ascii_only

        #: The high score table, on disk and outliving the session.
        self.scores = scores or ScoreBook(self.SCORE_KEY)
        #: Best bank reached *this run*, for the HUD. Not the record — that is
        #: on the board, and only what you leave the dome with goes on it.
        self.best_bank = 0
        #: What the player last typed, so a second run does not ask again.
        self.initials = "AAA"
        #: Where the last recorded run landed, for the ending screen.
        self.last_rank: int | None = None

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
        """Track the highest the bank has been *within* this run.

        Not a score. It moves every time a hand resolves, and what goes on the
        board is what you actually leave with — see :meth:`finish`.
        """
        self.best_bank = max(self.best_bank, amount)

    @property
    def best(self) -> int:
        """The best run on record, not just this session's."""
        return self.scores.best()

    def qualifies(self, wealth: int) -> bool:
        return wealth > 0 and self.scores.qualifies(wealth)

    def finish(self, wealth: int, rounds: int = 0, escaped: bool = False) -> None:
        """The run is over. Ask about it, or just record it.

        The moment that counts is leaving the dome, not losing a hand. Both
        ways out reach here: escaping banks the chips, and busting on the ante
        keeps whatever was banked already.

        **What is recorded is total wealth, not the bank**, because that is
        what the browser build files under this key —
        ``totalScore: this.totalWealth`` in js/state.js, which is
        ``chips + bank``. At the end of a run the two agree, since escaping
        banks the chips and busting leaves none; but recording the other one
        would put a different quantity on a shared board under the same name,
        which is the kind of thing nobody notices until the numbers are wrong.
        """
        if self.qualifies(wealth):
            self.enter_initials(wealth, rounds, escaped)
        elif wealth > 0:
            self.record(wealth, rounds=rounds, escaped=escaped)

    def record(self, wealth: int, initials: str | None = None,
               rounds: int = 0, escaped: bool = False):
        """Put a run on the board, with the extras the browser also keeps."""
        self.initials = initials or self.initials
        result = self.scores.save(self.initials, wealth,
                                  rounds=rounds, escaped=escaped)
        self.last_rank = result.rank
        return result

    def show_scores(self) -> None:
        """The high score table, over the title screen."""
        from lavadome.scenes import ScoresScene

        self.host.push_scene(ScoresScene(self))

    def enter_initials(self, wealth: int, rounds: int = 0,
                       escaped: bool = False) -> None:
        """Ask who just did that, over the ending."""
        from lavadome.scenes import InitialsScene

        self.host.push_scene(InitialsScene(self, wealth, rounds, escaped))

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
    from magmacrunch.engine.core.tui_host import TuiHost

    from lavadome.arcade import GAME

    host = TuiHost(title=GAME.info.title, fps=GAME.info.fps,
                   hold_ms=GAME.info.hold_ms)
    app = LavaDomeApp(host, seed, ascii_only)
    host.push_scene(app.root_scene)
    if skip_title:
        host.push_scene(GameScene(app))
    host.run()
