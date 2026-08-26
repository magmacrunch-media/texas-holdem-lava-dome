// betting.js - Texas Hold'Em Lava Dome
// Manages bet sizing, raises, folds, and the cash-out decision

class Betting {
    constructor(state) {
        this.state = state;
    }

    // ── Bet constraints ──────────────────────────────────────

    get minBet() {
        return Math.min(MIN_BET, this.state.chips);
    }

    get maxBet() {
        return this.state.chips; // Can go all-in
    }

    get suggestedBets() {
        // Offer 4 quick-pick amounts: min, 25%, 50%, all-in
        const chips = this.state.chips;
        const options = [
            this.minBet,
            Math.max(this.minBet, Math.floor(chips * 0.25)),
            Math.max(this.minBet, Math.floor(chips * 0.50)),
            chips
        ];
        // Deduplicate while preserving order
        return [...new Set(options)];
    }

    // ── Place initial bet (pre-flop) ─────────────────────────

    placeBet(amount) {
        const validated = this._validateBet(amount);
        if (!validated.ok) return validated;

        this.state.removeChips(validated.amount);
        this.state.currentBet = validated.amount;

        return {
            ok:          true,
            bet:         validated.amount,
            chipsRemaining: this.state.chips,
            totalBet:    this.state.currentBet
        };
    }

    // ── Raise (add more to existing bet mid-street) ──────────

    raise(amount) {
        const validated = this._validateBet(amount);
        if (!validated.ok) return validated;

        this.state.removeChips(validated.amount);
        this.state.currentBet += validated.amount;

        return {
            ok:          true,
            raised:      validated.amount,
            chipsRemaining: this.state.chips,
            totalBet:    this.state.currentBet
        };
    }

    // ── Check (no additional bet, advance street for free) ───

    check() {
        // Only valid if no bet has been placed yet this street,
        // or as a call when the cost is zero
        return {
            ok:       true,
            checked:  true,
            totalBet: this.state.currentBet
        };
    }

    // ── Fold ─────────────────────────────────────────────────

    fold() {
        // Forfeits current bet — dome.js handles the resolution
        return {
            ok:       true,
            folded:   true,
            lost:     this.state.currentBet
        };
    }

    // ── Cash-out decisions ───────────────────────────────────
    // Called during the cashout phase after a hand resolves

    cashOutPartial(amount) {
        const validated = this._validateCashOut(amount);
        if (!validated.ok) return validated;

        const actual = this.state.cashOut(validated.amount);

        return {
            ok:             true,
            cashed:         actual,
            bank:           this.state.bank,
            chipsRemaining: this.state.chips
        };
    }

    cashOutAll() {
        const actual = this.state.cashOutAll();
        return {
            ok:             true,
            cashed:         actual,
            bank:           this.state.bank,
            chipsRemaining: 0
        };
    }

    withdrawFromBank(amount) {
        const amt = parseInt(amount);
        if (isNaN(amt) || amt <= 0) {
            return { ok: false, error: 'Withdrawal must be a positive number.' };
        }
        if (amt > this.state.bank) {
            return { ok: false, error: `Not enough in bank. You have ${this.state.bank}.` };
        }
        const actual = this.state.withdrawFromBank(amt);
        return {
            ok:              true,
            withdrawn:       actual,
            bank:            this.state.bank,
            chips:           this.state.chips,
            chipsAfterAnte:  this.state.chips - this.state.currentAnte
        };
    }

    get suggestedWithdrawals() {
        const bank = this.state.bank;
        if (bank <= 0) return [];
        const options = [
            Math.floor(bank * 0.25),
            Math.floor(bank * 0.50),
            Math.floor(bank * 0.75),
            bank
        ];
        return [...new Set(options.filter(v => v > 0))];
    }

    // Suggested cash-out amounts: 25%, 50%, 75%, all
    get suggestedCashOuts() {
        const chips = this.state.chips;
        if (chips <= 0) return [];

        const options = [
            Math.floor(chips * 0.25),
            Math.floor(chips * 0.50),
            Math.floor(chips * 0.75),
            chips
        ];
        return [...new Set(options.filter(v => v > 0))];
    }

    // ── Bet sizing summary for UI ────────────────────────────

    get bettingStatus() {
        return {
            chips:         this.state.chips,
            bank:          this.state.bank,
            currentBet:    this.state.currentBet,
            minBet:        this.minBet,
            maxBet:        this.maxBet,
            suggestedBets: this.suggestedBets,
            suggestedWithdrawals: this.suggestedWithdrawals,
            canCheck:      this.state.currentBet === 0,
            canRaise:      this.state.chips > 0,
            canFold:       this.state.currentBet > 0,
            phase:         this.state.phase
        };
    }

    // ── Risk assessment ──────────────────────────────────────
    // Gives the UI something to show as a warning

    riskLevel(betAmount) {
        const pct = betAmount / this.state.chips;
        if (pct >= 1.0)  return { level: 'all-in',  label: 'ALL IN',       color: 'yellow' };
        if (pct >= 0.75) return { level: 'high',    label: 'HIGH RISK',    color: 'orange' };
        if (pct >= 0.40) return { level: 'medium',  label: 'MEDIUM RISK',  color: 'orange' };
        return              { level: 'low',     label: 'LOW RISK',     color: 'green'  };
    }

    // ── Validation ───────────────────────────────────────────

    _validateBet(amount) {
        const amt = parseInt(amount);

        if (isNaN(amt) || amt <= 0) {
            return { ok: false, error: 'Bet must be a positive number.' };
        }
        if (amt < this.minBet) {
            return { ok: false, error: `Minimum bet is ${this.minBet} chips.` };
        }
        if (amt > this.state.chips) {
            return { ok: false, error: `Not enough chips. You have ${this.state.chips}.` };
        }

        return { ok: true, amount: amt };
    }

    _validateCashOut(amount) {
        const amt = parseInt(amount);

        if (isNaN(amt) || amt <= 0) {
            return { ok: false, error: 'Cash-out amount must be positive.' };
        }
        if (amt > this.state.chips) {
            return { ok: false, error: `Can't cash out more than your chip stack (${this.state.chips}).` };
        }

        return { ok: true, amount: amt };
    }
}
