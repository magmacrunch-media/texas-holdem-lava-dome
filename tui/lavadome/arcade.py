"""What the arcade needs to know to launch this game.

The entry point a launcher resolves. Kept import-light on purpose: a menu
listing several games loads one of these per installed game just to draw a row,
and should not pay for this game's rules, its evaluator or its screens to do it.
"""

from __future__ import annotations

from typing import Any

from magmacrunch.engine.arcade import GameInfo

from lavadome import theme

#: The key this game's scoreboard is filed under. **Not** :attr:`INFO.key`.
#:
#: The web build has posted under ``solitaire-thld`` since before the rename,
#: and a shared board later has to mean a shared board rather than two with
#: different names.
#:
#: It lives here, in the import-light module, rather than in ``app.py`` where
#: it used to: the arcade reads it to draw this cabinet's best score on the
#: menu, and must not have to import the evaluator and the screens to learn a
#: string. ``LavaDomeApp.SCORE_KEY`` reads it from here, so the two places that
#: need it cannot drift apart.
SCORE_KEY = "solitaire-thld"

INFO = GameInfo(
    key="thld",
    title="Texas Hold'Em Lava Dome",
    blurb="Solo hold'em against a threshold that climbs every round.",
    # Turn-based: the table only changes on a keypress, and edge input is what
    # keeps one arrow press from running the bet across the whole stack.
    fps=20,
    hold_ms=0,
    min_cols=theme.MIN_COLS,
    min_rows=theme.MIN_ROWS,
    # Without this the arcade opens `thld.json`, finds nothing, and draws no
    # high score on this cabinet's card - forever, and with no symptom, because
    # an unfound scoreboard and an unplayed game look exactly alike.
    score_key=SCORE_KEY,
)


class LavaDomeGame:
    """Satisfies :class:`magmacrunch.engine.arcade.ArcadeGame`."""

    info = INFO

    def start(self, host: Any) -> Any:
        """The title screen, ready to be pushed.

        Imported here rather than at module scope so listing this game in an
        arcade menu does not drag in the evaluator or Textual.
        """
        from lavadome.app import LavaDomeApp

        return LavaDomeApp(host).root_scene


#: What the entry point resolves to. Stateless — a run's state belongs to the
#: LavaDomeApp that :meth:`LavaDomeGame.start` creates.
GAME = LavaDomeGame()

__all__ = ["GAME", "INFO", "SCORE_KEY", "LavaDomeGame"]
