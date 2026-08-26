"""The deck and the four streets.

A port of ``web/js/dealer.js``. Two deliberate differences from it, both
matching what the Wii port already does and both recorded in
``wii/README.md``:

* **No ace-high restamp.** The web build restamps every dealt card in
  ``_draw()`` because adenosine's ``Deck`` builds ace-*low* cards. Ours are
  ace-high at construction (see :mod:`lavadome.cards`), so there is no restamp
  step to forget at one of the four streets.
* **No burn cards.** The web build discards one card before the flop, the turn
  and the river. Off a freshly shuffled deck that cannot move any odds, so it
  is not reproduced. It would matter only for replaying a seed against the
  browser, which nothing does — and it *cannot* be done anyway, since the two
  use different random number generators.
"""

from __future__ import annotations

import random

from lavadome.cards import Deck
from lavadome.state import GameState


class Dealer:
    """Deals a Hold'Em hand, one street at a time."""

    def __init__(self, state: GameState, rng: random.Random | None = None):
        self.state = state
        self.rng = rng or random.Random()

    # ── Round setup ─────────────────────────────────────────────────

    def new_round(self) -> None:
        """Fresh deck, cleared table, and this round's dome numbers fixed.

        The threshold is *stored* rather than recomputed at resolution: the
        round number can change underneath a hand otherwise, and the player was
        told a number when they bet.
        """
        self.state.deck = Deck(self.rng)
        self.state.deck.shuffle()

        self.state.hole_cards = []
        self.state.community_cards = []
        self.state.best_hand = None
        self.state.current_bet = 0
        self.state.pot_win = 0
        self.state.beat_dome = False

        self.state.ante = self.state.current_ante
        self.state.dome_threshold = self.state.current_dome_threshold

    def _draw(self):
        card = self.state.deck.deal()
        if card is not None:
            card.face_up = True
        return card

    # ── Streets ─────────────────────────────────────────────────────

    def deal_hole_cards(self) -> bool:
        if self.state.hole_cards:
            return False
        for _ in range(2):
            card = self._draw()
            if card is not None:
                self.state.hole_cards.append(card)
        self.state.phase = "betting"
        return True

    def deal_flop(self) -> bool:
        if self.state.community_cards:
            return False
        for _ in range(3):
            card = self._draw()
            if card is not None:
                self.state.community_cards.append(card)
        self.state.phase = "flop"
        return True

    def deal_turn(self) -> bool:
        if len(self.state.community_cards) != 3:
            return False
        card = self._draw()
        if card is not None:
            self.state.community_cards.append(card)
        self.state.phase = "turn"
        return True

    def deal_river(self) -> bool:
        if len(self.state.community_cards) != 4:
            return False
        card = self._draw()
        if card is not None:
            self.state.community_cards.append(card)
        self.state.phase = "river"
        return True

    def advance_street(self) -> bool:
        """Move to the next street from wherever the hand is now."""
        phase = self.state.phase
        if phase == "betting":
            return self.deal_flop()
        if phase == "flop":
            return self.deal_turn()
        if phase == "turn":
            return self.deal_river()
        if phase == "river":
            self.state.phase = "resolve"
            return True
        return False

    # ── Fold ────────────────────────────────────────────────────────

    def fold(self) -> dict:
        """Forfeit the bet and skip resolution."""
        self.state.pot_win = -self.state.current_bet
        self.state.beat_dome = False
        self.state.phase = "cashout"
        return {"folded": True, "chips_lost": self.state.current_bet}

    # ── Labels ──────────────────────────────────────────────────────

    @property
    def community_label(self) -> str:
        return {0: "Pre-Flop", 3: "Flop", 4: "Turn", 5: "River"}.get(
            len(self.state.community_cards), "")

    @property
    def next_action_label(self) -> str:
        return {
            "betting": "Deal Flop",
            "flop": "Deal Turn",
            "turn": "Deal River",
            "river": "Resolve Hand",
        }.get(self.state.phase, "")

    @property
    def street_count(self) -> int:
        """How many betting opportunities have passed, 0-4."""
        return {"betting": 0, "flop": 1, "turn": 2,
                "river": 3, "resolve": 4}.get(self.state.phase, 0)
