"""Session and round state.

A port of ``web/js/state.js``, which was already free of any browser
dependency — the whole of ``state``, ``config``, ``dome`` and ``betting`` is,
which is why this game was cheap to bring to a terminal.

Two chip pools, and the difference between them is the game: ``chips`` are in
play and can be lost, ``bank`` is cashed out and safe. The bank is the score.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from lavadome import config

#: idle → betting → flop → turn → river → resolve → cashout
PHASES = ("idle", "betting", "flop", "turn", "river", "resolve", "cashout")


@dataclass
class GameState:
    # ── Session ─────────────────────────────────────────────────────
    chips: int = config.STARTING_CHIPS   # active betting stack
    bank: int = 0                        # permanent cashed-out score
    round: int = 0                       # 1-based once playing
    session_over: bool = False
    escaped: bool = False                # left voluntarily, rather than bust

    # ── Round ───────────────────────────────────────────────────────
    phase: str = "idle"
    ante: int = 0
    current_bet: int = 0
    pot_win: int = 0                     # chips won (+) or lost (-) last round

    # ── Cards ───────────────────────────────────────────────────────
    hole_cards: list[Any] = field(default_factory=list)
    community_cards: list[Any] = field(default_factory=list)
    deck: Any = None

    # ── Result ──────────────────────────────────────────────────────
    best_hand: Any = None
    dome_threshold: int = 0              # fixed at round start, not re-derived
    beat_dome: bool = False

    def reset(self) -> None:
        self.chips = config.STARTING_CHIPS
        self.bank = 0
        self.round = 0
        self.session_over = False
        self.escaped = False
        self.phase = "idle"
        self.ante = 0
        self.current_bet = 0
        self.pot_win = 0
        self.hole_cards = []
        self.community_cards = []
        self.deck = None
        self.best_hand = None
        self.dome_threshold = 0
        self.beat_dome = False

    # ── Derived ─────────────────────────────────────────────────────

    @property
    def total_wealth(self) -> int:
        return self.chips + self.bank

    @property
    def current_ante(self) -> int:
        return config.ante_for_round(self.round)

    @property
    def current_dome_threshold(self) -> int:
        return config.threshold_for_round(self.round)

    @property
    def current_depth_label(self) -> str:
        return config.depth_label(self.round)

    @property
    def all_cards(self) -> list[Any]:
        return [*self.hole_cards, *self.community_cards]

    @property
    def can_escape(self) -> bool:
        return self.phase == "cashout" and self.chips > 0

    @property
    def is_bust(self) -> bool:
        return self.chips <= 0

    # ── Chips ───────────────────────────────────────────────────────

    def cash_out(self, amount: int) -> int:
        """Move chips to the bank. Returns how many actually moved."""
        actual = min(amount, self.chips)
        self.chips -= actual
        self.bank += actual
        return actual

    def cash_out_all(self) -> int:
        return self.cash_out(self.chips)

    def add_chips(self, amount: int) -> None:
        self.chips += amount

    def remove_chips(self, amount: int) -> bool:
        """Take chips off the stack. Returns True if that emptied it."""
        self.chips = max(0, self.chips - amount)
        return self.chips == 0

    def withdraw_from_bank(self, amount: int) -> int:
        """Put banked chips back in play — the only way money moves backwards."""
        actual = min(amount, self.bank)
        self.bank -= actual
        self.chips += actual
        return actual

    # ── Serialization ───────────────────────────────────────────────

    def to_score_record(self, initials: str) -> dict:
        return {
            "initials": initials,
            "bank": self.bank,
            "chips": self.chips,
            "totalScore": self.total_wealth,
            "rounds": self.round,
            "escaped": self.escaped,
        }
