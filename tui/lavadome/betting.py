"""Bet sizing, raises, folds, and the cash-out decision.

A port of ``web/js/betting.js``. The validation returns result dicts rather
than raising, exactly as the JS does, because every one of these is driven by
a player action and an invalid one is a message on screen, not an error.

Dropped: ``riskLevel()``, which returned a CSS colour name. The terminal picks
its own colours from the fraction of the stack at risk.
"""

from __future__ import annotations

from lavadome import config
from lavadome.state import GameState


def _ok(**fields) -> dict:
    return {"ok": True, **fields}


def _err(message: str) -> dict:
    return {"ok": False, "error": message}


class Betting:
    """Everything the player can do with chips."""

    def __init__(self, state: GameState):
        self.state = state

    # ── Constraints ─────────────────────────────────────────────────

    @property
    def min_bet(self) -> int:
        # Capped by the stack: a player with 4 chips can still bet them.
        return min(config.MIN_BET, self.state.chips)

    @property
    def max_bet(self) -> int:
        return self.state.chips        # all-in is always allowed

    @property
    def suggested_bets(self) -> list[int]:
        """Four quick picks: minimum, a quarter, a half, everything."""
        chips = self.state.chips
        options = [
            self.min_bet,
            max(self.min_bet, chips // 4),
            max(self.min_bet, chips // 2),
            chips,
        ]
        return list(dict.fromkeys(options))     # dedupe, keep order

    # ── Betting ─────────────────────────────────────────────────────

    def place_bet(self, amount) -> dict:
        validated = self._validate_bet(amount)
        if not validated["ok"]:
            return validated

        self.state.remove_chips(validated["amount"])
        self.state.current_bet = validated["amount"]
        return _ok(bet=validated["amount"],
                   chips_remaining=self.state.chips,
                   total_bet=self.state.current_bet)

    def raise_bet(self, amount) -> dict:
        """Add to the standing bet.

        Named ``raise_bet`` because ``raise`` is a keyword. In the web build a
        raise also buys the next street — that is the caller's job, here as
        there, and the Wii README explains why: letting a raise stay on its
        street would be an open betting loop, which is a different and much
        longer game than the arcade one.
        """
        validated = self._validate_bet(amount)
        if not validated["ok"]:
            return validated

        self.state.remove_chips(validated["amount"])
        self.state.current_bet += validated["amount"]
        return _ok(raised=validated["amount"],
                   chips_remaining=self.state.chips,
                   total_bet=self.state.current_bet)

    def check(self) -> dict:
        """Take the next street without adding to the bet."""
        return _ok(checked=True, total_bet=self.state.current_bet)

    def fold(self) -> dict:
        """Give up the bet. The dealer moves the hand to cashout."""
        return _ok(folded=True, lost=self.state.current_bet)

    # ── Banking ─────────────────────────────────────────────────────

    def cash_out_partial(self, amount) -> dict:
        validated = self._validate_cash_out(amount)
        if not validated["ok"]:
            return validated
        actual = self.state.cash_out(validated["amount"])
        return _ok(cashed=actual, bank=self.state.bank,
                   chips_remaining=self.state.chips)

    def cash_out_all(self) -> dict:
        actual = self.state.cash_out_all()
        return _ok(cashed=actual, bank=self.state.bank, chips_remaining=0)

    def withdraw_from_bank(self, amount) -> dict:
        amt = _as_int(amount)
        if amt is None or amt <= 0:
            return _err("Withdrawal must be a positive number.")
        if amt > self.state.bank:
            return _err(f"Not enough in bank. You have {self.state.bank}.")
        actual = self.state.withdraw_from_bank(amt)
        return _ok(withdrawn=actual, bank=self.state.bank, chips=self.state.chips,
                   chips_after_ante=self.state.chips - self.state.current_ante)

    @property
    def suggested_withdrawals(self) -> list[int]:
        bank = self.state.bank
        if bank <= 0:
            return []
        options = [bank // 4, bank // 2, (bank * 3) // 4, bank]
        return list(dict.fromkeys(v for v in options if v > 0))

    @property
    def suggested_cash_outs(self) -> list[int]:
        chips = self.state.chips
        if chips <= 0:
            return []
        options = [chips // 4, chips // 2, (chips * 3) // 4, chips]
        return list(dict.fromkeys(v for v in options if v > 0))

    # ── Status ──────────────────────────────────────────────────────

    @property
    def status(self) -> dict:
        return {
            "chips": self.state.chips,
            "bank": self.state.bank,
            "current_bet": self.state.current_bet,
            "min_bet": self.min_bet,
            "max_bet": self.max_bet,
            "suggested_bets": self.suggested_bets,
            "suggested_withdrawals": self.suggested_withdrawals,
            "can_check": self.state.current_bet == 0,
            "can_raise": self.state.chips > 0,
            "can_fold": self.state.current_bet > 0,
            "phase": self.state.phase,
        }

    def risk_fraction(self, bet: int) -> float:
        """How much of the stack a bet puts at risk, 0..1."""
        if self.state.chips <= 0:
            return 1.0
        return min(1.0, bet / self.state.chips)

    # ── Validation ──────────────────────────────────────────────────

    def _validate_bet(self, amount) -> dict:
        amt = _as_int(amount)
        if amt is None or amt <= 0:
            return _err("Bet must be a positive number.")
        if amt < self.min_bet:
            return _err(f"Minimum bet is {self.min_bet} chips.")
        if amt > self.state.chips:
            return _err(f"Not enough chips. You have {self.state.chips}.")
        return _ok(amount=amt)

    def _validate_cash_out(self, amount) -> dict:
        amt = _as_int(amount)
        if amt is None or amt <= 0:
            return _err("Cash-out amount must be positive.")
        if amt > self.state.chips:
            return _err(
                f"Can't cash out more than your chip stack ({self.state.chips}).")
        return _ok(amount=amt)


def _as_int(value) -> int | None:
    """``parseInt``-ish: a number, or None where the JS would give NaN."""
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return None
