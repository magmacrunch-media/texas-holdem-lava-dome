"""What the arcade needs to know to launch this game.

The entry point a launcher resolves. Kept import-light on purpose: a menu
listing several games loads one of these per installed game just to draw a row,
and should not pay for this game's rules, its evaluator or its screens to do it.
"""

from __future__ import annotations

from typing import Any

from magmacrunch.engine.arcade import GameInfo

from lavadome import theme

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

__all__ = ["GAME", "INFO", "LavaDomeGame"]
