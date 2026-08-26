"""Poker hand evaluation — the best 5-card hand from 2 to 7 cards.

A port of ``adenosine/packages/cards/src/hand-eval.ts``, which is the source of
truth the Wii port's README names and which itself says *"From Texas Hold'Em
Lava Dome"* — this game is where it came from originally.

It is ported line for line rather than rewritten, because a poker evaluator
that is subtly wrong plays *almost* right, which is worse than one that plainly
does not. ``tests/test_handeval.py`` checks it against the real JavaScript over
thousands of random hands (see ``tools/`` and the README), which is the same
method the web build's ``config.js`` records using when this evaluator replaced
the game's original one.

**The caller supplies ``value``, and it is never rewritten here.** That is what
lets one evaluator serve ace-low and ace-high games. Poker is ace-high, so
cards must come from :mod:`lavadome.cards`, which stamps ace-high at
construction.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from itertools import combinations
from typing import Any

HAND_RANKS = {
    "Royal Flush": 9,
    "Straight Flush": 8,
    "Four of a Kind": 7,
    "Full House": 6,
    "Flush": 5,
    "Straight": 4,
    "Three of a Kind": 3,
    "Two Pair": 2,
    "One Pair": 1,
    "High Card": 0,
}

HAND_POINTS = {
    "Royal Flush": 1000,
    "Straight Flush": 500,
    "Four of a Kind": 250,
    "Full House": 150,
    "Flush": 100,
    "Straight": 75,
    "Three of a Kind": 50,
    "Two Pair": 25,
    "One Pair": 10,
    "High Card": 0,
}


@dataclass
class HandResult:
    """The outcome of evaluating a hand.

    ``points`` is what the dome compares against its threshold, and ``name`` is
    what the payout table is keyed on — those two fields are the game. The rest
    is for display and for breaking ties between candidate five-card hands.
    """

    name: str
    rank: int
    points: int
    tiebreakers: list[int] = field(default_factory=list)
    cards: list[Any] = field(default_factory=list)
    description: str = ""
    partial: bool = False


def _rank_name_for(sorted_cards: list[Any], value: int) -> str:
    for card in sorted_cards:
        if card.value == value:
            return card.rank
    return sorted_cards[0].rank if sorted_cards else ""


class HandEvaluator:
    """Evaluates the best five-card hand out of the cards it is given."""

    def evaluate(self, cards: list[Any]) -> HandResult:
        if not cards or len(cards) < 2:
            return self._empty_result()
        if len(cards) < 5:
            return self._evaluate_partial(cards)

        best: HandResult | None = None
        for combo in combinations(cards, 5):
            result = self._evaluate_five(list(combo))
            # Strictly greater, so the first candidate wins a tie — same as the
            # JS, which matters only for which cards come back, never the name.
            if best is None or self._compare(result, best) > 0:
                best = result
        return best if best is not None else self._empty_result()

    # ── Five-card evaluation ────────────────────────────────────────

    def _evaluate_five(self, cards: list[Any]) -> HandResult:
        ordered = sorted(cards, key=lambda c: c.value, reverse=True)
        is_flush = self._is_flush(cards)
        is_straight = self._is_straight(ordered)
        counts = self._value_counts(cards)
        count_vals = sorted(counts.values(), reverse=True)
        first = count_vals[0] if count_vals else 0
        second = count_vals[1] if len(count_vals) > 1 else 0

        if is_flush and is_straight and ordered[0].value == 14 and ordered[1].value == 13:
            name = "Royal Flush"
            tiebreakers = [14]
        elif is_flush and is_straight:
            name = "Straight Flush"
            tiebreakers = [self._straight_high_card(ordered)]
        elif first == 4:
            name = "Four of a Kind"
            tiebreakers = self._tiebreak_by_count(counts, [4, 1])
        elif first == 3 and second == 2:
            name = "Full House"
            tiebreakers = self._tiebreak_by_count(counts, [3, 2])
        elif is_flush:
            name = "Flush"
            tiebreakers = [c.value for c in ordered]
        elif is_straight:
            name = "Straight"
            tiebreakers = [self._straight_high_card(ordered)]
        elif first == 3:
            name = "Three of a Kind"
            tiebreakers = self._tiebreak_by_count(counts, [3, 1, 1])
        elif first == 2 and second == 2:
            name = "Two Pair"
            tiebreakers = self._tiebreak_by_count(counts, [2, 2, 1])
        elif first == 2:
            name = "One Pair"
            tiebreakers = self._tiebreak_by_count(counts, [2, 1, 1, 1])
        else:
            name = "High Card"
            tiebreakers = [c.value for c in ordered]

        return HandResult(
            name=name,
            rank=HAND_RANKS[name],
            points=HAND_POINTS[name],
            tiebreakers=tiebreakers,
            cards=ordered,
            description=self._describe(name, ordered),
        )

    def _evaluate_partial(self, cards: list[Any]) -> HandResult:
        """Name a two-to-four card hand, for showing progress before the river.

        The order of these checks is the JS order, not a tidied one: Two Pair is
        tested before Full House and Four of a Kind. With fewer than five cards
        the later branches cannot both match, so it comes to the same thing —
        but reordering it would be a change in behaviour on a hand of five, and
        this function is not the one that guards against being called with one.
        """
        ordered = sorted(cards, key=lambda c: c.value, reverse=True)
        counts = self._value_counts(cards)
        count_vals = sorted(counts.values(), reverse=True)
        first = count_vals[0] if count_vals else 0
        second = count_vals[1] if len(count_vals) > 1 else 0

        name = "High Card"
        if first == 2 and second == 2:
            name = "Two Pair"
        elif first == 3 and second == 2:
            name = "Full House"
        elif first == 4:
            name = "Four of a Kind"
        elif first == 3:
            name = "Three of a Kind"
        elif first == 2:
            name = "One Pair"

        return HandResult(
            name=name,
            rank=HAND_RANKS[name],
            points=HAND_POINTS[name],
            tiebreakers=[c.value for c in ordered],
            cards=ordered,
            description=f"{name} (partial)",
            partial=True,
        )

    @staticmethod
    def _empty_result() -> HandResult:
        return HandResult(
            name="No Cards",
            rank=-1,
            points=0,
            tiebreakers=[],
            cards=[],
            description="No cards dealt",
            partial=True,
        )

    # ── Comparison ──────────────────────────────────────────────────

    @staticmethod
    def _compare(a: HandResult, b: HandResult) -> int:
        if a.rank != b.rank:
            return a.rank - b.rank
        for i in range(max(len(a.tiebreakers), len(b.tiebreakers))):
            av = a.tiebreakers[i] if i < len(a.tiebreakers) else 0
            bv = b.tiebreakers[i] if i < len(b.tiebreakers) else 0
            if av != bv:
                return av - bv
        return 0

    # ── Shapes ──────────────────────────────────────────────────────

    @staticmethod
    def _is_flush(cards: list[Any]) -> bool:
        suit = cards[0].suit
        return all(c.suit == suit for c in cards)

    @staticmethod
    def _is_straight(ordered: list[Any]) -> bool:
        """Five in sequence, counting the wheel.

        A-2-3-4-5 is a straight even though the ace sorts to the front, which is
        the one case a plain descending-run check misses.
        """
        if all(ordered[i].value - ordered[i + 1].value == 1
               for i in range(len(ordered) - 1)):
            return True

        values = sorted(c.value for c in ordered)
        return (len(values) == 5 and values[4] == 14
                and values[0] == 2 and values[1] == 3
                and values[2] == 4 and values[3] == 5)

    @staticmethod
    def _straight_high_card(ordered: list[Any]) -> int:
        """The rank a straight is named for.

        In the wheel the ace plays low, so the hand is five high even though the
        ace sorts to the front.
        """
        values = sorted(c.value for c in ordered)
        if (len(values) == 5 and values[4] == 14
                and values[0] == 2 and values[3] == 5):
            return 5
        return ordered[0].value

    def _straight_rank_name(self, ordered: list[Any]) -> str:
        return _rank_name_for(ordered, self._straight_high_card(ordered))

    @staticmethod
    def _value_counts(cards: list[Any]) -> dict[int, int]:
        counts: dict[int, int] = {}
        for card in cards:
            counts[card.value] = counts.get(card.value, 0) + 1
        return counts

    @staticmethod
    def _tiebreak_by_count(counts: dict[int, int], pattern: list[int]) -> list[int]:
        """Rank values ordered by how many of each there are.

        ``pattern`` is the shape being described — ``[3, 2]`` for a full house
        means "the tripled rank, then the paired one" — so a full house of
        threes over kings beats one of twos over aces.
        """
        groups: dict[int, list[int]] = {}
        for value, count in counts.items():
            groups.setdefault(count, []).append(value)
        for group in groups.values():
            group.sort(reverse=True)

        result: list[int] = []
        seen: set[int] = set()
        for target in pattern:
            for value in groups.get(target, ()):
                if value not in seen:
                    result.append(value)
                    seen.add(value)
                    break
        return result

    # ── Description ─────────────────────────────────────────────────

    def _describe(self, name: str, ordered: list[Any]) -> str:
        top = ordered[0]
        if name == "Royal Flush":
            return f"Royal Flush — {top.suit}"
        if name == "Straight Flush":
            return f"Straight Flush — {self._straight_rank_name(ordered)} high"
        if name == "Four of a Kind":
            return f"Four {top.rank}s"
        if name == "Full House":
            counts = self._value_counts(ordered)
            # sorted() for the same reason as Two Pair below. With five cards
            # there is only ever one of each, so this is belt and braces.
            triple = next((v for v, c in sorted(counts.items()) if c == 3), None)
            pair = next((v for v, c in sorted(counts.items()) if c == 2), None)
            if triple is None or pair is None:
                return "Full House"
            return (f"Full House — {_rank_name_for(ordered, triple)}s full of "
                    f"{_rank_name_for(ordered, pair)}s")
        if name == "Flush":
            return f"Flush — {top.rank} high ({top.suit})"
        if name == "Straight":
            return f"Straight — {self._straight_rank_name(ordered)} high"
        if name == "Three of a Kind":
            return f"Three {top.rank}s"
        if name == "Two Pair":
            counts = self._value_counts(ordered)
            # sorted(): a JS object with integer-like keys enumerates in
            # ascending numeric order no matter how it was built, where a
            # Python dict keeps insertion order — which here is descending
            # card order. Without this, kings and fives read "Ks and 5s"
            # where every other port says "5s and Ks".
            pairs = "s and ".join(
                _rank_name_for(ordered, v)
                for v, c in sorted(counts.items()) if c == 2
            )
            return f"Two Pair — {pairs}s"
        if name == "One Pair":
            counts = self._value_counts(ordered)
            pair = next((v for v, c in sorted(counts.items()) if c == 2), None)
            return f"Pair of {_rank_name_for(ordered, pair) if pair else ''}s"
        if name == "High Card":
            return f"{top.rank} high"
        return name
