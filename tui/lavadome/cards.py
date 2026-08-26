"""Cards and a deck — the parts of adenosine-cards this game actually uses.

The web build imports `AdCards.Deck`, but only ever touches four fields on a
card: suit, rank, value and face_up. Everything else on the JS `Card` is
rendering — `getHTML()`, `FACE_CARD_SVG`, `SUIT_SYMBOLS` — and none of it means
anything in a terminal. So this is a reimplementation of the ~20 lines that
matter, not a port of the package.

**Aces are high here.** This is the trap the Wii port's README calls out and
`js/dealer.js` carries a paragraph about: adenosine's default `RANK_VALUES` is
ace-*low* (A = 1), because cribbage counts an ace as one and solitaire builds
up from the ace. The evaluator reads ``value`` off the cards it is handed and
never rewrites it, so a poker game dealing straight from an ace-low deck scores
aces as the *lowest* card in the deck: a royal flush grades as an ordinary
flush, broadway does not register as a straight at all, and a pair of aces
loses to a pair of twos.

The web build fixes that by restamping every dealt card in `Dealer._draw()`.
This port does what the Wii port does and defines ace-high once, here, so there
is no restamp step to forget.
"""

from __future__ import annotations

import random
from dataclasses import dataclass

SUITS = ("hearts", "diamonds", "clubs", "spades")
RANKS = ("A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K")

SUIT_SYMBOLS = {"hearts": "♥", "diamonds": "♦", "clubs": "♣", "spades": "♠"}
#: Terminals vary in how they render the solid suit glyphs; these always work.
SUIT_LETTERS = {"hearts": "H", "diamonds": "D", "clubs": "C", "spades": "S"}
RED_SUITS = frozenset({"hearts", "diamonds"})

#: Ace-high. See the module docstring for why this is not the adenosine default.
POKER_RANK_VALUES = {
    "A": 14, "2": 2, "3": 3, "4": 4, "5": 5, "6": 6, "7": 7,
    "8": 8, "9": 9, "10": 10, "J": 11, "Q": 12, "K": 13,
}


def poker_value(rank: str) -> int:
    return POKER_RANK_VALUES[rank]


@dataclass
class Card:
    """One playing card, already valued ace-high."""

    suit: str
    rank: str
    value: int = 0
    face_up: bool = True

    def __post_init__(self) -> None:
        if not self.value:
            self.value = poker_value(self.rank)

    @property
    def is_red(self) -> bool:
        return self.suit in RED_SUITS

    @property
    def symbol(self) -> str:
        return SUIT_SYMBOLS[self.suit]

    @property
    def letter(self) -> str:
        return SUIT_LETTERS[self.suit]

    def label(self, *, ascii_only: bool = False) -> str:
        """``A♠`` — or ``AS`` where the suit glyphs will not render."""
        return f"{self.rank}{self.letter if ascii_only else self.symbol}"

    def __str__(self) -> str:
        return self.label()


class Deck:
    """A standard 52-card deck.

    ``rng`` is injected rather than using the module-level ``random`` so a
    session can be replayed exactly from a seed — which is what makes the
    round-flow tests deterministic without stubbing the dealer.
    """

    def __init__(self, rng: random.Random | None = None):
        self.rng = rng or random.Random()
        self.cards: list[Card] = []
        self.build()

    def build(self) -> None:
        self.cards = [Card(suit, rank) for suit in SUITS for rank in RANKS]

    def shuffle(self) -> None:
        self.rng.shuffle(self.cards)

    def deal(self) -> Card | None:
        """Take the top card, or None when the deck is spent.

        Returns None rather than raising because that is what ``Array.pop()``
        does on the JS side, and a burn card off an empty deck must not be a
        crash. Nothing in a 52-card game gets close, but the round flow deals
        without checking and this keeps that honest.
        """
        return self.cards.pop() if self.cards else None

    def __len__(self) -> int:
        return len(self.cards)
