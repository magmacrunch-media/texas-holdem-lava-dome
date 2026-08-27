"""The dome: ante escalation, hand resolution, bust and escape.

A port of ``web/js/dome.js``, which the repo's ``AGENTS.md`` names as source of
truth for round flow alongside ``state.js``.

One deliberate difference from the web build, and it is a bug fix rather than a
taste call: there, ``resolveHand()`` is invoked from ``_phaseResolve()`` — a
*render* function. It works only because resolving sets the phase to
``cashout``, so the next render cannot reach it again. A render that awards
chips is a trap for anything that ever needs to redraw, and a terminal redraws
constantly (every resize, every frame). Here resolution happens on the
transition into the phase, and rendering only reads.
"""

from __future__ import annotations

import random

from lavadome import config
from lavadome.handeval import HandEvaluator
from lavadome.state import GameState


class Dome:
    """Charges the ante, grades the hand, and decides when the session ends."""

    def __init__(self, state: GameState, evaluator: HandEvaluator | None = None,
                 rng: random.Random | None = None):
        self.state = state
        self.evaluator = evaluator or HandEvaluator()
        self.rng = rng or random.Random()

    # ── Round start ─────────────────────────────────────────────────

    def charge_ante(self) -> dict:
        """Take this round's ante. Busts if it cannot be paid."""
        ante = self.state.current_ante

        if self.state.chips <= 0:
            return self._bust("No chips remaining before ante.")

        # Short of the full ante, the player pays what they have — and is then
        # out of chips, which is a bust.
        actual = min(ante, self.state.chips)
        self.state.ante = actual
        self.state.remove_chips(actual)

        if self.state.chips == 0:
            return self._bust("Ante consumed remaining chips.")

        return {"ok": True, "ante": actual, "chips_remaining": self.state.chips}

    # ── Resolution ──────────────────────────────────────────────────

    def resolve_hand(self) -> dict:
        """Grade the hand against the threshold and settle the bet."""
        result = self.evaluator.evaluate(self.state.all_cards)
        self.state.best_hand = result

        threshold = self.state.dome_threshold
        beat = result.points >= threshold
        self.state.beat_dome = beat

        chips_won = 0
        chips_lost = 0

        if beat:
            multiplier = config.PAYOUT_MULTIPLIERS.get(result.name, 1)
            chips_won = int(self.state.current_bet * multiplier)
            # The stake comes back *and* the winnings on top.
            self.state.add_chips(self.state.current_bet + chips_won)
            flavor = self._flavor(config.FLAVOR_WIN_BIG)
        else:
            chips_lost = self.state.current_bet
            flavor = self._flavor(config.FLAVOR_BUST)

        self.state.pot_win = chips_won if beat else -chips_lost
        self.state.phase = "cashout"

        return {
            "hand_name": result.name,
            "description": result.description,
            "points": result.points,
            "threshold": threshold,
            "beat_dome": beat,
            "chips_won": chips_won,
            "chips_lost": chips_lost,
            "net_chips": self.state.pot_win,
            "chips_now": self.state.chips,
            "bank": self.state.bank,
            "flavor": flavor,
            "bust": self.state.chips <= 0,
        }

    # ── Banking and leaving ─────────────────────────────────────────

    def cash_out(self, amount: int | None = None) -> dict:
        """Bank some chips, or all of them when ``amount`` is None."""
        actual = (self.state.cash_out_all() if amount is None
                  else self.state.cash_out(amount))
        return {
            "cashed": actual,
            "bank": self.state.bank,
            "chips_remaining": self.state.chips,
        }

    def escape(self) -> dict:
        """End the session voluntarily, banking everything still in play."""
        self.cash_out()
        self.state.escaped = True
        self.state.session_over = True
        return {
            "escaped": True,
            "final_bank": self.state.bank,
            "rounds": self.state.round,
            "flavor": self._flavor(config.FLAVOR_ESCAPE),
            "depth_label": self.state.current_depth_label,
        }

    def _bust(self, reason: str = "") -> dict:
        self.state.session_over = True
        self.state.escaped = False
        return {
            "bust": True,
            "reason": reason,
            "final_bank": self.state.bank,
            "rounds": self.state.round,
            "flavor": self._flavor(config.FLAVOR_BUST),
            "depth_label": self.state.current_depth_label,
        }

    def start_next_round(self) -> dict:
        """Descend. Busts when there is nothing left anywhere."""
        if self.state.chips <= 0 and self.state.bank <= 0:
            return self._bust("No chips and no bank remaining.")

        self.state.round += 1
        self.state.phase = "idle"

        return {
            "ok": True,
            "round": self.state.round,
            "ante": self.state.current_ante,
            "threshold": self.state.current_dome_threshold,
            "depth_label": self.state.current_depth_label,
            "chips": self.state.chips,
            "bank": self.state.bank,
        }

    # ── Status ──────────────────────────────────────────────────────

    @property
    def status(self) -> dict:
        return {
            "round": self.state.round,
            "ante": self.state.current_ante,
            "threshold": self.state.current_dome_threshold,
            "depth_label": self.state.current_depth_label,
            "chips": self.state.chips,
            "bank": self.state.bank,
            "phase": self.state.phase,
            "can_escape": self.state.can_escape,
            "is_bust": self.state.is_bust,
        }

    def ante_preview(self, rounds_ahead: int = 1) -> list[tuple[int, int]]:
        """``[(round, ante), …]`` for the next few rounds, to warn with."""
        return [(self.state.round + i, config.ante_for_round(self.state.round + i))
                for i in range(1, rounds_ahead + 1)]

    def _flavor(self, lines: tuple[str, ...]) -> str:
        return lines[self.rng.randrange(len(lines))]
