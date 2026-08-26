// state.js - Texas Hold'Em Lava Dome game state

class GameState {
    constructor() {
        this.reset();
    }

    reset() {
        // ── Session-level state ──────────────────────────────
        this.chips       = STARTING_CHIPS; // Active betting stack
        this.bank        = 0;              // Permanent cashed-out score
        this.round       = 0;              // Current round number (1-based when playing)
        this.sessionOver = false;
        this.escaped     = false;          // True if player chose to escape vs bust

        // ── Round-level state ────────────────────────────────
        this.phase       = 'idle';         // idle | betting | flop | turn | river | resolve | cashout
        this.ante        = 0;              // Ante paid this round
        this.currentBet  = 0;             // Total bet committed this round
        this.potWin      = 0;              // Chips won/lost last round resolution

        // ── Cards ────────────────────────────────────────────
        this.holeCards      = [];          // Player's 2 private cards
        this.communityCards = [];          // Up to 5 shared cards [flop x3, turn, river]
        this.deck           = null;        // Active Deck instance (set by Dealer)

        // ── Hand result ──────────────────────────────────────
        this.bestHand       = null;        // { name, cards, points } from last evaluation
        this.domeThreshold  = 0;           // Points needed to beat the dome this round
        this.beatDome       = false;       // Did the player beat the dome?

        // High scores are loaded once from JSONBin — not cleared on reset
    }

    // ── Derived getters ──────────────────────────────────────

    get totalWealth() {
        return this.chips + this.bank;
    }

    get currentAnte() {
        const idx = Math.min(this.round - 1, ANTE_SCHEDULE.length - 1);
        if (this.round - 1 < ANTE_SCHEDULE.length) {
            return ANTE_SCHEDULE[idx];
        }
        // Beyond schedule: last value + escalation per extra round
        const extra = (this.round - 1) - (ANTE_SCHEDULE.length - 1);
        return ANTE_SCHEDULE[ANTE_SCHEDULE.length - 1] + (extra * ANTE_ESCALATION_RATE);
    }

    get currentDomeThreshold() {
        return DOME_BASE_THRESHOLD + (this.round - 1) * DOME_THRESHOLD_SCALE;
    }

    get currentDepthLabel() {
        // Find the highest depth label whose round <= current round
        let label = DOME_DEPTHS[0];
        for (const depth of DOME_DEPTHS) {
            if (this.round >= depth.round) label = depth;
        }
        return label;
    }

    get allCards() {
        return [...this.holeCards, ...this.communityCards];
    }

    get canEscape() {
        return this.phase === 'cashout' && this.chips > 0;
    }

    get isBust() {
        return this.chips <= 0;
    }

    // ── Chip operations ──────────────────────────────────────

    cashOut(amount) {
        const actual = Math.min(amount, this.chips);
        this.chips -= actual;
        this.bank  += actual;
        return actual;
    }

    cashOutAll() {
        return this.cashOut(this.chips);
    }

    addChips(amount) {
        this.chips += amount;
    }

    removeChips(amount) {
        this.chips = Math.max(0, this.chips - amount);
        return this.chips === 0;
    }

    withdrawFromBank(amount) {
        const actual = Math.min(amount, this.bank);
        this.bank  -= actual;
        this.chips += actual;
        return actual;
    }

    // ── Serialization (for high score storage) ───────────────

    toScoreRecord(initials) {
        return {
            initials,
            bank:       this.bank,
            chips:      this.chips,
            totalScore: this.totalWealth,
            rounds:     this.round,
            escaped:    this.escaped
        };
    }
}
