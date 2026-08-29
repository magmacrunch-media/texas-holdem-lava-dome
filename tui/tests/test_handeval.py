"""Hand evaluation, checked against the JavaScript it was ported from.

The web build has no test suite to carry over — the Wii port's README says so
explicitly, and calls it out as a problem: *"the rules cannot be checked by
agreeing with a reference — something else has to stand in for one."*

Something else stands in here. `tools/js_oracle.mjs` loads the actual shipped
`adenosine-cards.js` bundle in node, and the differential tests below run both
evaluators over thousands of random hands and compare every field. That is the
same method `web/js/config.js` records using when AdCards replaced this game's
original evaluator: *"Verified behaviourally identical over 20,000 random 2-7
card hands."*

The hand-written cases come first and run everywhere. The differential tests
skip when node is unavailable, so a machine without it still checks the shapes
that matter — they just cannot prove agreement.
"""

from __future__ import annotations

import json
import os
import random
import shutil
import subprocess
from pathlib import Path

import pytest

from lavadome.cards import RANKS, SUITS, Card, poker_value
from lavadome.handeval import HAND_POINTS, HAND_RANKS, HandEvaluator

ORACLE = Path(__file__).resolve().parent.parent / "tools" / "js_oracle.mjs"


def _oracle_source() -> str | None:
    """Which JavaScript the oracle would use, or None if it has none.

    Asked of the oracle rather than guessed here: it knows about both the npm
    package and the vendored bundle, and the two live in completely different
    places. Guessing paths in Python is how this ends up skipping silently on a
    machine where it could in fact have run.
    """
    if shutil.which("node") is None or not ORACLE.exists():
        return None
    try:
        proc = subprocess.run(
            ["node", str(ORACLE), "--check"],
            capture_output=True, text=True, timeout=60, encoding="utf-8",
        )
    except (OSError, subprocess.SubprocessError):
        return None
    return proc.stdout.strip() if proc.returncode == 0 else None


ORACLE_SOURCE = _oracle_source()

requires_oracle = pytest.mark.skipif(
    ORACLE_SOURCE is None,
    reason=("no JavaScript evaluator to compare against — install node and run "
            "`npm install --prefix tui/tools`"),
)


@pytest.fixture(scope="module")
def evaluator():
    return HandEvaluator()


def hand(*specs: str) -> list[Card]:
    """``hand("As", "Kh")`` — rank then a one-letter suit."""
    suits = {"s": "spades", "h": "hearts", "d": "diamonds", "c": "clubs"}
    return [Card(suits[spec[-1]], spec[:-1]) for spec in specs]


# ── The shapes ──────────────────────────────────────────────────────


@pytest.mark.parametrize(
    "cards,name",
    [
        (("As", "Ks", "Qs", "Js", "10s"), "Royal Flush"),
        (("9s", "8s", "7s", "6s", "5s"), "Straight Flush"),
        (("As", "2s", "3s", "4s", "5s"), "Straight Flush"),   # the wheel
        (("As", "Ah", "Ad", "Ac", "Ks"), "Four of a Kind"),
        (("As", "Ah", "Ad", "Ks", "Kh"), "Full House"),
        (("As", "Js", "9s", "6s", "3s"), "Flush"),
        (("As", "Kh", "Qd", "Jc", "10s"), "Straight"),        # broadway
        (("As", "2h", "3d", "4c", "5s"), "Straight"),         # the wheel again
        (("As", "Ah", "Ad", "Ks", "Qh"), "Three of a Kind"),
        (("As", "Ah", "Ks", "Kh", "Qd"), "Two Pair"),
        (("As", "Ah", "Ks", "Qh", "Jd"), "One Pair"),
        (("As", "Kh", "Qd", "Jc", "9s"), "High Card"),
    ],
)
def test_every_hand_shape_is_named(evaluator, cards, name):
    assert evaluator.evaluate(hand(*cards)).name == name


def test_ranks_and_points_agree_with_the_shared_tables():
    assert HAND_RANKS["Royal Flush"] == 9
    assert HAND_RANKS["High Card"] == 0
    assert HAND_POINTS["Royal Flush"] == 1000
    # High Card pays nothing, so it cannot beat the dome at any threshold —
    # the Wii README flags this as easy to port wrong.
    assert HAND_POINTS["High Card"] == 0


# ── Aces are high ───────────────────────────────────────────────────
#
# The single most consequential thing to get wrong here. adenosine's default
# RANK_VALUES is ace-LOW, and the evaluator reads value off the card without
# rewriting it, so an ace-low deck scores a royal flush as an ordinary flush.


def test_an_ace_is_fourteen():
    assert poker_value("A") == 14
    assert Card("spades", "A").value == 14


def test_broadway_registers_as_a_straight():
    result = HandEvaluator().evaluate(hand("As", "Kh", "Qd", "Jc", "10s"))
    assert result.name == "Straight"
    assert result.tiebreakers == [14]


def test_a_pair_of_aces_beats_a_pair_of_twos():
    ev = HandEvaluator()
    aces = ev.evaluate(hand("As", "Ah", "9d", "7c", "5s"))
    twos = ev.evaluate(hand("2s", "2h", "9d", "7c", "5s"))
    assert ev._compare(aces, twos) > 0


def test_the_wheel_is_five_high_not_ace_high():
    # The ace plays low in A-2-3-4-5 even though it sorts to the front.
    result = HandEvaluator().evaluate(hand("As", "2h", "3d", "4c", "5s"))
    assert result.name == "Straight"
    assert result.tiebreakers == [5]
    assert "5 high" in result.description


def test_the_wheel_loses_to_a_six_high_straight():
    ev = HandEvaluator()
    wheel = ev.evaluate(hand("As", "2h", "3d", "4c", "5s"))
    six = ev.evaluate(hand("2s", "3h", "4d", "5c", "6s"))
    assert ev._compare(six, wheel) > 0


# ── Best-of-seven ───────────────────────────────────────────────────


def test_the_best_five_of_seven_is_found():
    # Two spare cards that must not drag the hand down.
    result = HandEvaluator().evaluate(
        hand("As", "Ks", "Qs", "Js", "10s", "2h", "3d"))
    assert result.name == "Royal Flush"


def test_a_full_house_is_preferred_to_the_flush_in_the_same_seven():
    result = HandEvaluator().evaluate(
        hand("As", "Ah", "Ad", "Ks", "Kh", "Qs", "Js"))
    assert result.name == "Full House"


# ── Partial hands ───────────────────────────────────────────────────


def test_two_cards_evaluate_as_partial():
    result = HandEvaluator().evaluate(hand("As", "Ah"))
    assert result.partial
    assert result.name == "One Pair"
    assert result.points == 10


def test_fewer_than_two_cards_is_no_hand():
    ev = HandEvaluator()
    assert ev.evaluate([]).name == "No Cards"
    assert ev.evaluate(hand("As")).name == "No Cards"
    assert ev.evaluate([]).points == 0


def test_four_cards_can_name_two_pair():
    result = HandEvaluator().evaluate(hand("As", "Ah", "Ks", "Kh"))
    assert result.name == "Two Pair"
    assert result.partial


# ── Descriptions ────────────────────────────────────────────────────


def test_two_pair_names_the_lower_pair_first():
    # A JS object with integer-like keys enumerates in ascending numeric order
    # regardless of insertion; a Python dict keeps insertion order. Without
    # matching that, this would read "Ks and 5s".
    result = HandEvaluator().evaluate(hand("Ks", "Kh", "5s", "5h", "9d"))
    assert result.description == "Two Pair — 5s and Ks"


def test_a_full_house_names_the_triple_then_the_pair():
    result = HandEvaluator().evaluate(hand("3s", "3h", "3d", "Ks", "Kh"))
    assert result.description == "Full House — 3s full of Ks"


# ── Differential: the Python must agree with the JavaScript ─────────


def _js_evaluate(hands: list[list[Card]]) -> list[dict]:
    payload = json.dumps([
        [{"suit": c.suit, "rank": c.rank, "value": c.value} for c in h]
        for h in hands
    ])
    proc = subprocess.run(
        ["node", str(ORACLE)],
        input=payload, capture_output=True, text=True, timeout=180,
        # node writes UTF-8; without this Python decodes it as the locale
        # encoding, and every em-dash in a description comes back as mojibake.
        encoding="utf-8",
    )
    if proc.returncode != 0:
        pytest.fail(f"js oracle failed:\n{proc.stdout}\n{proc.stderr}")
    return json.loads(proc.stdout)


def _random_hands(count: int, sizes=(2, 3, 4, 5, 6, 7), seed: int = 0):
    rng = random.Random(seed)
    deck = [Card(s, r) for s in SUITS for r in RANKS]
    hands = []
    for _ in range(count):
        hands.append(rng.sample(deck, rng.choice(sizes)))
    return hands


def _compare_batch(hands: list[list[Card]]) -> None:
    ev = HandEvaluator()
    js = _js_evaluate(hands)
    for cards, want in zip(hands, js, strict=True):
        got = ev.evaluate(cards)
        shown = " ".join(f"{c.rank}{c.suit[0]}" for c in cards)
        assert got.name == want["name"], f"name for [{shown}]"
        assert got.rank == want["rank"], f"rank for [{shown}]"
        assert got.points == want["points"], f"points for [{shown}]"
        assert got.tiebreakers == want["tiebreakers"], f"tiebreakers for [{shown}]"
        assert got.description == want["description"], f"description for [{shown}]"
        assert got.partial == want["partial"], f"partial for [{shown}]"


@requires_oracle
def test_the_oracle_loads_a_real_javascript_evaluator():
    assert ORACLE_SOURCE.startswith(("npm:", "bundle:")), ORACLE_SOURCE
    results = _js_evaluate([hand("As", "Ks", "Qs", "Js", "10s")])
    assert results[0]["name"] == "Royal Flush"
    assert results[0]["points"] == 1000


@pytest.mark.skipif(
    os.environ.get("LAVADOME_REQUIRE_ORACLE") != "1",
    reason="set LAVADOME_REQUIRE_ORACLE=1 to make a missing oracle a failure",
)
def test_the_oracle_is_available_when_ci_says_it_must_be():
    """CI sets LAVADOME_REQUIRE_ORACLE=1 so a missing oracle fails loudly.

    Skipping is right on a dev box with no node. In CI it would turn the one
    test that proves the port agrees with the original into a silent pass —
    the same trap texastoast's conftest guarded against with
    TEXASTOAST_REQUIRE_TK, back when this game ran on that engine.
    """
    assert ORACLE_SOURCE is not None, (
        "LAVADOME_REQUIRE_ORACLE=1 but no JavaScript evaluator was found; "
        "the differential tests would have skipped and reported green"
    )


@requires_oracle
@pytest.mark.parametrize("size", [2, 3, 4, 5, 6, 7])
def test_agrees_with_the_javascript_at_every_hand_size(size):
    _compare_batch(_random_hands(400, sizes=(size,), seed=size))


@requires_oracle
def test_agrees_with_the_javascript_over_mixed_random_hands():
    _compare_batch(_random_hands(2000, seed=1234))


@requires_oracle
def test_agrees_with_the_javascript_on_every_five_card_flush():
    # Flushes and straight flushes are rare in random sampling, so the two
    # highest-paying hands would otherwise go essentially unchecked.
    hands = []
    for suit in SUITS:
        cards = [Card(suit, r) for r in RANKS]
        for i in range(len(cards) - 4):
            hands.append(cards[i:i + 5])
        hands.append([cards[0]] + cards[1:5])          # the wheel, suited
    _compare_batch(hands)


@requires_oracle
def test_agrees_with_the_javascript_on_engineered_tie_breaks():
    # Random hands almost never produce two candidates of the same rank inside
    # one seven-card set, which is exactly where _compare and the tiebreak
    # patterns earn their keep.
    hands = [
        hand("As", "Ah", "Ks", "Kh", "Qs", "Qh", "2d"),   # three pairs
        hand("As", "Ah", "Ad", "Ks", "Kh", "Qs", "Qh"),   # house, two pairs
        hand("2s", "2h", "2d", "2c", "3s", "3h", "3d"),   # quads + trips
        hand("As", "Ks", "Qs", "Js", "10s", "9s", "8s"),  # seven suited
        hand("As", "2h", "3d", "4c", "5s", "6h", "7d"),   # wheel and up
        hand("As", "Ah", "2d", "2c", "3s", "3h", "4d"),   # low three pairs
        hand("Ks", "Kh", "Kd", "Kc", "As", "Ah", "Ad"),   # quads over trips
    ]
    _compare_batch(hands)


# ── The rules stand alone ───────────────────────────────────────────


def test_evaluation_needs_no_engine():
    import subprocess
    import sys

    result = subprocess.run(
        [sys.executable, "-c",
         "import sys; from lavadome.handeval import HandEvaluator; "
         "from lavadome.cards import Card; "
         "print(HandEvaluator().evaluate([Card('spades','A'), Card('hearts','A')]).name, "
         "'magmacrunch' in sys.modules, 'textual' in sys.modules)"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "One Pair False False"
