"""The dome, the dealer and the betting rules.

Ported from `web/js/` — `config.js`, `state.js`, `dome.js`, `betting.js` and
`dealer.js` — which the repo's AGENTS.md names as source of truth. The web
build has no test suite of its own, so unlike the George Boole port there is no
assertion table to carry over; these are written against the JS behaviour
directly, with the Wii port's README as a second reading of the same rules.

Nothing here imports texastoast or Textual.
"""

from __future__ import annotations

import random

import pytest

from lavadome import config
from lavadome.betting import Betting
from lavadome.cards import Card, Deck
from lavadome.dealer import Dealer
from lavadome.dome import Dome
from lavadome.handeval import HandEvaluator
from lavadome.state import GameState


@pytest.fixture
def state():
    return GameState()


def table(state, seed=7):
    rng = random.Random(seed)
    return (Dealer(state, rng), Dome(state, HandEvaluator(), rng), Betting(state))


def hand(*specs: str) -> list[Card]:
    suits = {"s": "spades", "h": "hearts", "d": "diamonds", "c": "clubs"}
    return [Card(suits[spec[-1]], spec[:-1]) for spec in specs]


# ── The tuning tables ───────────────────────────────────────────────


@pytest.mark.parametrize(
    "round_number,ante",
    [(1, 10), (2, 10), (3, 20), (7, 50), (13, 150), (14, 200), (15, 250)],
)
def test_the_ante_schedule(round_number, ante):
    assert config.ante_for_round(round_number) == ante


def test_the_ante_climbs_past_the_end_of_the_schedule():
    # Round 15 is the last entry; every round after adds the escalation rate.
    assert config.ante_for_round(16) == 250 + 25
    assert config.ante_for_round(20) == 250 + 25 * 5


@pytest.mark.parametrize(
    "round_number,threshold", [(1, 10), (2, 15), (5, 30), (10, 55)]
)
def test_the_threshold_climbs_with_the_round(round_number, threshold):
    assert config.threshold_for_round(round_number) == threshold


def test_depth_labels_hold_until_the_next_one():
    assert config.depth_label(1) == "I keep my cards close to my heart"
    assert config.depth_label(3) == config.depth_label(4)
    # Past the table the deepest label stays.
    assert config.depth_label(99) == "All All & All"


def test_a_high_card_cannot_beat_the_dome_at_any_threshold():
    # Two things make this true and both are load-bearing: it scores 0 points,
    # and it pays 0 even if it somehow cleared a threshold of 0.
    from lavadome.handeval import HAND_POINTS

    assert HAND_POINTS["High Card"] == 0
    assert config.PAYOUT_MULTIPLIERS["High Card"] == 0


# ── The ante ────────────────────────────────────────────────────────


def test_the_ante_comes_off_the_stack(state):
    _, dome, _ = table(state)
    state.round = 1
    result = dome.charge_ante()
    assert result["ok"]
    assert result["ante"] == 10
    assert state.chips == config.STARTING_CHIPS - 10


def test_an_ante_that_empties_the_stack_is_a_bust(state):
    _, dome, _ = table(state)
    state.round = 1
    state.chips = 10                    # exactly the ante
    result = dome.charge_ante()
    assert result["bust"]
    assert state.session_over


def test_a_partial_ante_takes_what_is_there_and_busts(state):
    _, dome, _ = table(state)
    state.round = 15                    # 250 ante
    state.chips = 40
    result = dome.charge_ante()
    assert result["bust"]
    assert state.chips == 0


def test_no_chips_at_all_busts_before_charging(state):
    _, dome, _ = table(state)
    state.round = 1
    state.chips = 0
    assert dome.charge_ante()["bust"]


# ── Resolution ──────────────────────────────────────────────────────


def test_beating_the_dome_returns_the_stake_and_pays_the_multiplier(state):
    _, dome, _ = table(state)
    state.round = 1
    state.dome_threshold = 10
    state.chips = 100
    state.current_bet = 40
    state.hole_cards = hand("As", "Ah")
    state.community_cards = hand("Kd", "9c", "4s", "2h", "7d")

    result = dome.resolve_hand()
    assert result["beat_dome"]
    assert result["hand_name"] == "One Pair"
    # One Pair pays 1x: the 40 stake back plus 40 won.
    assert result["chips_won"] == 40
    assert state.chips == 100 + 40 + 40
    assert state.phase == "cashout"


def test_missing_the_threshold_forfeits_the_bet(state):
    _, dome, _ = table(state)
    state.round = 8
    state.dome_threshold = 45           # needs better than a pair
    state.chips = 100
    state.current_bet = 40
    state.hole_cards = hand("As", "Kh")
    state.community_cards = hand("9d", "7c", "4s", "2h", "3d")

    result = dome.resolve_hand()
    assert not result["beat_dome"]
    assert result["chips_lost"] == 40
    assert state.chips == 100           # the stake was already taken
    assert state.pot_win == -40


def test_a_big_hand_pays_its_multiplier(state):
    _, dome, _ = table(state)
    state.dome_threshold = 10
    state.chips = 0
    state.current_bet = 100
    state.hole_cards = hand("As", "Ks")
    state.community_cards = hand("Qs", "Js", "10s", "2h", "3d")

    result = dome.resolve_hand()
    assert result["hand_name"] == "Royal Flush"
    assert result["chips_won"] == 1000          # 100 * 10
    assert state.chips == 100 + 1000            # stake back plus winnings


def test_the_multiplier_truncates_rather_than_rounding(state):
    # Two Pair is 1.25x, and the JS uses Math.floor.
    _, dome, _ = table(state)
    state.dome_threshold = 10
    state.chips = 0
    state.current_bet = 10
    state.hole_cards = hand("As", "Ah")
    state.community_cards = hand("Ks", "Kh", "9d", "4c", "2s")
    result = dome.resolve_hand()
    assert result["hand_name"] == "Two Pair"
    assert result["chips_won"] == 12            # 12.5 floored


def test_the_threshold_used_is_the_one_fixed_at_round_start(state):
    # Not re-derived from the round number: the player was told a number when
    # they bet, and the round can advance underneath a hand.
    dealer, dome, _ = table(state)
    state.round = 1
    dealer.new_round()
    assert state.dome_threshold == 10
    state.round = 10                    # would be 55 if re-derived
    state.current_bet = 10
    state.hole_cards = hand("As", "Ah")
    state.community_cards = hand("Kd", "9c", "4s", "2h", "7d")
    assert dome.resolve_hand()["threshold"] == 10


# ── Banking, escape, bust ───────────────────────────────────────────


def test_cashing_out_moves_chips_to_the_bank(state):
    _, dome, _ = table(state)
    state.chips = 300
    dome.cash_out(120)
    assert state.chips == 180
    assert state.bank == 120


def test_cashing_out_more_than_the_stack_takes_only_the_stack(state):
    _, dome, _ = table(state)
    state.chips = 50
    result = dome.cash_out(500)
    assert result["cashed"] == 50
    assert state.chips == 0
    assert state.bank == 50


def test_escaping_banks_everything_and_ends_the_session(state):
    _, dome, _ = table(state)
    state.chips = 220
    state.bank = 80
    result = dome.escape()
    assert result["escaped"]
    assert result["final_bank"] == 300
    assert state.session_over
    assert state.escaped
    assert state.chips == 0


def test_the_next_round_busts_only_when_chips_and_bank_are_both_gone(state):
    _, dome, _ = table(state)
    state.chips = 0
    state.bank = 100
    assert dome.start_next_round()["ok"]     # the bank can still be withdrawn

    state.chips = 0
    state.bank = 0
    assert dome.start_next_round()["bust"]


def test_starting_the_next_round_descends(state):
    _, dome, _ = table(state)
    state.round = 3
    state.chips = 100
    result = dome.start_next_round()
    assert result["round"] == 4
    assert result["ante"] == config.ante_for_round(4)
    assert state.phase == "idle"


def test_escape_is_only_offered_in_the_cashout_phase(state):
    state.chips = 100
    state.phase = "betting"
    assert not state.can_escape
    state.phase = "cashout"
    assert state.can_escape
    state.chips = 0
    assert not state.can_escape


# ── Dealing ─────────────────────────────────────────────────────────


def test_a_round_deals_two_hole_cards_then_five_community(state):
    dealer, _, _ = table(state)
    state.round = 1
    dealer.new_round()
    dealer.deal_hole_cards()
    assert len(state.hole_cards) == 2
    assert state.phase == "betting"

    dealer.advance_street()
    assert len(state.community_cards) == 3
    assert state.phase == "flop"

    dealer.advance_street()
    assert len(state.community_cards) == 4
    assert state.phase == "turn"

    dealer.advance_street()
    assert len(state.community_cards) == 5
    assert state.phase == "river"

    dealer.advance_street()
    assert state.phase == "resolve"


def test_every_dealt_card_is_ace_high(state):
    # The single most consequential thing to get wrong. Ours are ace-high at
    # construction, so unlike the web build there is no restamp to forget.
    dealer, _, _ = table(state)
    dealer.new_round()
    dealer.deal_hole_cards()
    for _ in range(3):
        dealer.advance_street()
    for card in state.all_cards:
        if card.rank == "A":
            assert card.value == 14


def test_no_card_is_ever_dealt_twice(state):
    dealer, _, _ = table(state)
    dealer.new_round()
    dealer.deal_hole_cards()
    for _ in range(3):
        dealer.advance_street()
    seen = [(c.suit, c.rank) for c in state.all_cards]
    assert len(seen) == 7
    assert len(set(seen)) == 7


def test_dealing_out_of_order_is_refused(state):
    dealer, _, _ = table(state)
    dealer.new_round()
    assert not dealer.deal_turn()           # no flop yet
    assert not dealer.deal_river()
    dealer.deal_hole_cards()
    assert not dealer.deal_hole_cards()     # already dealt


def test_a_new_round_clears_the_table(state):
    dealer, _, _ = table(state)
    state.round = 1
    dealer.new_round()
    dealer.deal_hole_cards()
    dealer.advance_street()
    state.current_bet = 50

    state.round = 2
    dealer.new_round()
    assert state.hole_cards == []
    assert state.community_cards == []
    assert state.current_bet == 0
    assert state.ante == config.ante_for_round(2)


def test_folding_forfeits_the_bet_and_skips_resolution(state):
    dealer, _, _ = table(state)
    state.current_bet = 75
    result = dealer.fold()
    assert result["chips_lost"] == 75
    assert state.phase == "cashout"
    assert not state.beat_dome


def test_a_seeded_deal_is_reproducible():
    def deal(seed):
        s = GameState()
        d = Dealer(s, random.Random(seed))
        d.new_round()
        d.deal_hole_cards()
        for _ in range(3):
            d.advance_street()
        return [(c.suit, c.rank) for c in s.all_cards]

    assert deal(42) == deal(42)
    assert deal(42) != deal(43)


def test_a_deck_holds_fifty_two_distinct_cards():
    deck = Deck(random.Random(1))
    assert len(deck) == 52
    assert len({(c.suit, c.rank) for c in deck.cards}) == 52


def test_an_empty_deck_deals_nothing_rather_than_raising():
    deck = Deck(random.Random(1))
    for _ in range(52):
        assert deck.deal() is not None
    assert deck.deal() is None


# ── Betting ─────────────────────────────────────────────────────────


def test_a_bet_leaves_the_stack_and_becomes_the_wager(state):
    _, _, betting = table(state)
    state.chips = 200
    result = betting.place_bet(50)
    assert result["ok"]
    assert state.chips == 150
    assert state.current_bet == 50


def test_a_raise_adds_to_the_standing_bet(state):
    _, _, betting = table(state)
    state.chips = 200
    betting.place_bet(50)
    betting.raise_bet(30)
    assert state.current_bet == 80
    assert state.chips == 120


@pytest.mark.parametrize("amount", [0, -5, "", "abc", None])
def test_a_nonsense_bet_is_refused(state, amount):
    _, _, betting = table(state)
    result = betting.place_bet(amount)
    assert not result["ok"]
    assert state.chips == config.STARTING_CHIPS


def test_betting_more_than_the_stack_is_refused(state):
    _, _, betting = table(state)
    state.chips = 40
    result = betting.place_bet(100)
    assert not result["ok"]
    assert "Not enough chips" in result["error"]


def test_betting_below_the_minimum_is_refused(state):
    _, _, betting = table(state)
    state.chips = 500
    assert not betting.place_bet(5)["ok"]


def test_the_minimum_bet_is_capped_by_a_short_stack(state):
    # With 4 chips the minimum cannot be 10, or the player could never bet.
    _, _, betting = table(state)
    state.chips = 4
    assert betting.min_bet == 4
    assert betting.place_bet(4)["ok"]


def test_all_in_is_allowed(state):
    _, _, betting = table(state)
    state.chips = 137
    assert betting.place_bet(137)["ok"]
    assert state.chips == 0


def test_suggested_bets_are_distinct_and_within_the_stack(state):
    _, _, betting = table(state)
    state.chips = 500
    suggestions = betting.suggested_bets
    assert suggestions == sorted(suggestions)
    assert len(suggestions) == len(set(suggestions))
    assert max(suggestions) == 500
    assert min(suggestions) >= betting.min_bet


def test_a_tiny_stack_collapses_the_suggestions_to_one(state):
    _, _, betting = table(state)
    state.chips = 8
    assert betting.suggested_bets == [8]


def test_withdrawing_from_the_bank_puts_chips_back_in_play(state):
    _, _, betting = table(state)
    state.chips = 20
    state.bank = 300
    result = betting.withdraw_from_bank(100)
    assert result["ok"]
    assert state.chips == 120
    assert state.bank == 200


def test_withdrawing_more_than_the_bank_is_refused(state):
    _, _, betting = table(state)
    state.bank = 50
    assert not betting.withdraw_from_bank(100)["ok"]
    assert state.bank == 50


def test_cash_out_suggestions_are_empty_with_no_chips(state):
    _, _, betting = table(state)
    state.chips = 0
    assert betting.suggested_cash_outs == []
    state.bank = 0
    assert betting.suggested_withdrawals == []


def test_risk_is_the_fraction_of_the_stack_at_stake(state):
    _, _, betting = table(state)
    state.chips = 200
    assert betting.risk_fraction(200) == 1.0
    assert betting.risk_fraction(50) == 0.25


# ── A whole round ───────────────────────────────────────────────────


def test_a_full_round_plays_through(state):
    dealer, dome, betting = table(state, seed=99)
    state.round = 1

    dealer.new_round()
    assert dome.charge_ante()["ok"]
    dealer.deal_hole_cards()

    assert betting.place_bet(50)["ok"]
    dealer.advance_street()             # flop
    dealer.advance_street()             # turn
    dealer.advance_street()             # river
    dealer.advance_street()             # resolve

    result = dome.resolve_hand()
    assert state.phase == "cashout"
    assert result["hand_name"]
    assert len(state.all_cards) == 7
    assert state.chips + state.bank >= 0


def test_the_bank_is_never_at_risk(state):
    # The whole point of banking: chips in the bank cannot be lost to the dome.
    dealer, dome, betting = table(state, seed=5)
    state.round = 1
    dealer.new_round()
    dome.charge_ante()
    dealer.deal_hole_cards()
    betting.place_bet(state.chips)      # all in
    dome.cash_out(0)
    state.bank = 250

    for _ in range(4):
        dealer.advance_street()
    dome.resolve_hand()
    assert state.bank == 250
