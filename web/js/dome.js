// dome.js - Texas Hold'Em Lava Dome
// The dome: ante escalation, hand resolution, bust/escape logic

class Dome {
    constructor(state, evaluator) {
        this.state = state;
        this.evaluator = evaluator;
    }

    // ── Round start ──────────────────────────────────────────
    // Charges the ante and checks if player can afford it
    // Returns { ok, ante, chips } or { bust } if player can't pay

    chargeAnte() {
        const ante = this.state.currentAnte;

        if (this.state.chips <= 0) {
            return this._bust('No chips remaining before ante.');
        }

        // If player can't cover full ante, they pay what they have
        const actual = Math.min(ante, this.state.chips);
        this.state.ante = actual;
        this.state.removeChips(actual);

        if (this.state.chips === 0) {
            return this._bust('Ante consumed remaining chips.');
        }

        return { ok: true, ante: actual, chipsRemaining: this.state.chips };
    }

    // ── Hand resolution ──────────────────────────────────────
    // Called after river — evaluates hand, compares to dome threshold,
    // applies winnings or losses, advances to cashout phase
    // Returns a full result object for the UI to display

    resolveHand() {
        const allCards = this.state.allCards;
        const result   = this.evaluator.evaluate(allCards);
        this.state.bestHand = result;

        const threshold  = this.state.domeThreshold;
        const beatDome   = result.points >= threshold;
        this.state.beatDome = beatDome;

        let chipsWon  = 0;
        let chipsLost = 0;
        let flavor    = '';

        if (beatDome) {
            // Win: return bet + payout based on hand strength
            const multiplier = PAYOUT_MULTIPLIERS[result.name] ?? 1;
            chipsWon = Math.floor(this.state.currentBet * multiplier);
            this.state.addChips(this.state.currentBet + chipsWon);
            flavor = this._randomFlavor(FLAVOR_WIN_BIG);
        } else {
            // Loss: forfeit the bet
            chipsLost = this.state.currentBet;
            flavor = this._randomFlavor(FLAVOR_BUST);
        }

        this.state.potWin = beatDome ? chipsWon : -chipsLost;
        this.state.phase  = 'cashout';

        return {
            handName:    result.name,
            description: result.description,
            points:      result.points,
            threshold,
            beatDome,
            chipsWon,
            chipsLost,
            netChips:    this.state.potWin,
            chipsNow:    this.state.chips,
            bank:        this.state.bank,
            flavor,
            bust:        this.state.chips <= 0
        };
    }

    // ── Cash out ─────────────────────────────────────────────
    // Moves some or all chips to the bank
    // amount = null means cash out everything

    cashOut(amount = null) {
        const actual = amount === null
            ? this.state.cashOutAll()
            : this.state.cashOut(amount);

        return {
            cashed:      actual,
            bank:        this.state.bank,
            chipsRemaining: this.state.chips
        };
    }

    // ── Escape ───────────────────────────────────────────────
    // Player voluntarily ends the session, cashing out everything

    escape() {
        this.cashOut(); // Cash out all remaining chips
        this.state.escaped     = true;
        this.state.sessionOver = true;

        return {
            escaped:    true,
            finalBank:  this.state.bank,
            rounds:     this.state.round,
            flavor:     this._randomFlavor(FLAVOR_ESCAPE),
            depthLabel: this.state.currentDepthLabel.label
        };
    }

    // ── Bust ─────────────────────────────────────────────────

    _bust(reason = '') {
        this.state.sessionOver = true;
        this.state.escaped     = false;

        return {
            bust:       true,
            reason,
            finalBank:  this.state.bank,
            rounds:     this.state.round,
            flavor:     this._randomFlavor(FLAVOR_BUST),
            depthLabel: this.state.currentDepthLabel.label
        };
    }

    // ── New round gate ───────────────────────────────────────
    // Call this to advance to the next round after cashout phase
    // Returns bust result if player is out of chips

    startNextRound() {
        if (this.state.chips <= 0 && this.state.bank <= 0) {
            return this._bust('No chips and no bank remaining.');
        }

        this.state.round++;
        this.state.phase = 'idle';

        return {
            ok:         true,
            round:      this.state.round,
            ante:       this.state.currentAnte,
            threshold:  this.state.currentDomeThreshold,
            depthLabel: this.state.currentDepthLabel,
            chips:      this.state.chips,
            bank:       this.state.bank
        };
    }

    // ── Dome status ──────────────────────────────────────────
    // Snapshot of current dome state for UI display

    get status() {
        return {
            round:      this.state.round,
            ante:       this.state.currentAnte,
            threshold:  this.state.currentDomeThreshold,
            depthLabel: this.state.currentDepthLabel,
            chips:      this.state.chips,
            bank:       this.state.bank,
            phase:      this.state.phase,
            canEscape:  this.state.canEscape,
            isBust:     this.state.isBust
        };
    }

    // ── Ante preview ─────────────────────────────────────────
    // How much will the ante be N rounds from now? Useful for UI warnings

    antePreview(roundsAhead = 1) {
        const previews = [];
        for (let i = 1; i <= roundsAhead; i++) {
            const r = this.state.round + i;
            const idx = Math.min(r - 1, ANTE_SCHEDULE.length - 1);
            let ante;
            if (r - 1 < ANTE_SCHEDULE.length) {
                ante = ANTE_SCHEDULE[idx];
            } else {
                const extra = (r - 1) - (ANTE_SCHEDULE.length - 1);
                ante = ANTE_SCHEDULE[ANTE_SCHEDULE.length - 1] + (extra * ANTE_ESCALATION_RATE);
            }
            previews.push({ round: r, ante });
        }
        return previews;
    }

    // ── Helpers ──────────────────────────────────────────────

    _randomFlavor(arr) {
        return arr[Math.floor(Math.random() * arr.length)];
    }
}
