// ui.js - Texas Hold'Em Lava Dome
// All DOM rendering, event handling, and screen state management

class UI {
    constructor(state, dealer, evaluator, dome, betting, scoring) {
        this.state     = state;
        this.dealer    = dealer;
        this.evaluator = evaluator;
        this.dome      = dome;
        this.betting   = betting;
        this.scoring   = scoring;

        this._pendingBet = 0;
    }

    // ── Bootstrap ────────────────────────────────────────────

    init() {
        this._buildGameScreenHTML();
        this._attachTopLevelListeners();
    }

    // ── Top-level screen switches ────────────────────────────

    showStartScreen() {
        document.getElementById('startScreen').style.display = '';
        document.getElementById('gameScreen').style.display  = 'none';
    }

    showGameScreen() {
        document.getElementById('startScreen').style.display = 'none';
        document.getElementById('gameScreen').style.display  = 'block';
        this.scoring.loadHighScores().then(() => this.scoring.displayHighScores());
        this._startSession();
    }

    // ── Session lifecycle ────────────────────────────────────

    _startSession() {
        this.state.reset();
        this.state.round = 1;
        this.render();
    }

    _startRound() {
        this.dealer.newRound();
        const anteResult = this.dome.chargeAnte();

        if (anteResult.bust) {
            this._handleSessionOver(anteResult);
            return;
        }

        this.dealer.dealHoleCards();
        this._pendingBet = 0;
        this.render();
    }

    // ── Master render ────────────────────────────────────────

    render() {
        this._renderHeader();
        this._renderDepthBar();
        this._renderCards();
        this._renderPhasePanel();
        this._renderHandStatus();
        ChipAnim.setChips(this.state.chips);
    }

    // ── Header ───────────────────────────────────────────────

    _renderHeader() {
        this._set('ui-chips', this._chipDisplay(this.state.chips));
        this._set('ui-bank',  this._chipDisplay(this.state.bank));
        this._set('ui-round', `R${this.state.round}`);
        this._set('ui-ante',  this.state.currentAnte);
    }

    // ── Depth bar ────────────────────────────────────────────

    _renderDepthBar() {
        const depth = this.state.currentDepthLabel;
        this._set('ui-depth-label', depth.label.toUpperCase());

        // Ante warning: flag if next round's ante is meaningfully higher
        const preview = this.dome.antePreview(1)[0];
        const warning = document.getElementById('ui-ante-warning');
        if (warning) {
            if (preview.ante > this.state.currentAnte) {
                warning.textContent = `⚠ ANTE RISES TO ${preview.ante} NEXT ROUND`;
                warning.style.display = '';
            } else {
                warning.style.display = 'none';
            }
        }
    }

    // ── Cards ────────────────────────────────────────────────

    _renderCards() {
        this._renderHoleCards();
        this._renderCommunityCards();
    }

    _renderHoleCards() {
        const el = document.getElementById('ui-hole-cards');
        if (!el) return;
        el.innerHTML = '';

        if (this.state.holeCards.length === 0) {
            el.innerHTML = '<div class="card-placeholder">?</div><div class="card-placeholder">?</div>';
            return;
        }

        this.state.holeCards.forEach(card => {
            card.faceUp = true;
            el.appendChild(card.getHTML());
        });
    }

    _renderCommunityCards() {
        const el = document.getElementById('ui-community-cards');
        if (!el) return;
        el.innerHTML = '';

        // Always show 5 slots
        for (let i = 0; i < 5; i++) {
            if (i < this.state.communityCards.length) {
                const card = this.state.communityCards[i];
                card.faceUp = true;
                el.appendChild(card.getHTML());
            } else {
                const placeholder = document.createElement('div');
                placeholder.className = 'card-placeholder';
                placeholder.textContent = i < 3 ? 'F' : (i === 3 ? 'T' : 'R');
                el.appendChild(placeholder);
            }
        }
    }

    // ── Phase panel ──────────────────────────────────────────
    // The main interactive area — changes completely based on phase

    _renderPhasePanel() {
        const panel = document.getElementById('ui-phase-panel');
        if (!panel) return;

        switch (this.state.phase) {
            case 'idle':    panel.innerHTML = this._phaseIdle();    break;
            case 'betting': panel.innerHTML = this._phaseBetting(); break;
            case 'flop':
            case 'turn':
            case 'river':   panel.innerHTML = this._phaseStreet();  break;
            case 'resolve': panel.innerHTML = this._phaseResolve(); break;
            case 'cashout': panel.innerHTML = this._phaseCashOut(); break;
            default:        panel.innerHTML = '';
        }

        this._attachPhaseListeners();
    }

    // Idle — between rounds
    _phaseIdle() {
        const ante      = this.state.currentAnte;
        const threshold = this.state.currentDomeThreshold;
        const preview   = this.dome.antePreview(2);
        const chips     = this.state.chips;
        const bank      = this.state.bank;
        const cantAffordAnte = chips < ante && bank > 0;

        return `
            <div class="phase-panel idle-panel">
                <div class="phase-title">ROUND ${this.state.round}</div>
                <div class="dome-stats">
                    <div class="dome-stat">
                        <span class="dome-stat-label">ANTE</span>
                        <span class="dome-stat-value">${ante}</span>
                    </div>
                    <div class="dome-stat">
                        <span class="dome-stat-label">DOME THRESHOLD</span>
                        <span class="dome-stat-value">${threshold} PTS</span>
                    </div>
                    <div class="dome-stat">
                        <span class="dome-stat-label">YOUR CHIPS</span>
                        <span class="dome-stat-value">${chips}</span>
                    </div>
                </div>
                <div class="ante-preview">
                    UPCOMING ANTES: ${preview.map(p => `R${p.round}→${p.ante}`).join(' &nbsp;|&nbsp; ')}
                </div>
                ${cantAffordAnte ? `
                <div class="ante-danger">
                    ⚠ YOU CAN'T AFFORD THE ${ante} ANTE — DIG INTO SAVINGS?
                </div>
                <div class="withdraw-label">WITHDRAW FROM BANK:</div>
                <div class="withdraw-quick-picks">
                    ${this.betting.suggestedWithdrawals.map(w => `
                        <button class="bet-quick-btn withdraw-quick-btn" data-amount="${w}">${w}</button>
                    `).join('')}
                </div>
                <div class="bet-custom">
                    <input type="number"
                           id="withdraw-input"
                           min="0"
                           max="${bank}"
                           value="0"
                           placeholder="Amount to withdraw">
                </div>
                ` : ''}
                <div class="phase-actions">
                    <button id="btn-start-round" class="action-btn primary" ${cantAffordAnte ? 'style="display:none"' : ''}>DEAL CARDS</button>
                    ${cantAffordAnte ? `
                    <button id="btn-withdraw-partial" class="action-btn">WITHDRAW & DEAL</button>
                    ` : ''}
                    <button id="btn-escape-pre" class="action-btn escape-btn" ${this.state.chips <= 0 && bank <= 0 ? 'disabled' : ''}>ESCAPE DOME</button>
                </div>
            </div>
        `;
    }

    // Betting — hole cards dealt, player sets their bet
    _phaseBetting() {
        const status = this.betting.bettingStatus;
        const bets   = this.betting.suggestedBets;
        const bank   = this.state.bank;
        const canWithdraw = bank > 0 && status.chips < status.minBet;

        return `
            <div class="phase-panel betting-panel">
                <div class="phase-title">PLACE YOUR BET</div>
                <div class="phase-subtitle">Pre-flop &mdash; ${status.chips} chips available</div>
                ${canWithdraw ? `
                <div class="ante-danger">
                    ⚠ NOT ENOUGH CHIPS TO BET MINIMUM (${status.minBet}) — DIG INTO SAVINGS?
                </div>
                <div class="withdraw-label">WITHDRAW FROM BANK:</div>
                <div class="withdraw-quick-picks">
                    ${this.betting.suggestedWithdrawals.map(w => `
                        <button class="bet-quick-btn withdraw-quick-btn" data-amount="${w}">${w}</button>
                    `).join('')}
                </div>
                <div class="bet-custom">
                    <input type="number"
                           id="withdraw-input"
                           min="0"
                           max="${bank}"
                           value="0"
                           placeholder="Amount to withdraw">
                </div>
                ` : ''}
                <div class="bet-total">BET: ${this._pendingBet}</div>
                <div class="bet-breakdown">${this._chipBreakdown(this._pendingBet)}</div>

                <div class="chip-denom-rows">
                    ${this._chipDenomRowsHTML(this._pendingBet, status.maxBet)}
                </div>

                <div class="bet-custom">
                    <input type="number"
                           id="bet-input"
                           min="${status.minBet}"
                           max="${status.maxBet}"
                           value="${this._pendingBet}"
                           placeholder="${status.minBet}">
                    <div class="risk-indicator" id="risk-indicator">
                        ${this._riskBadge(this._pendingBet)}
                    </div>
                </div>

                <div class="phase-actions">
                    ${canWithdraw ? `<button id="btn-withdraw-bet" class="action-btn">WITHDRAW & BET</button>` : ''}
                    <button id="btn-place-bet" class="action-btn primary">BET ${this._pendingBet}</button>
                    <button id="btn-check-preflop" class="action-btn">CHECK</button>
                </div>
            </div>
        `;
    }

    // Street — flop/turn/river, raise or advance
    _phaseStreet() {
        const status = this.betting.bettingStatus;
        const label  = this.dealer.communityLabel.toUpperCase();
        const next   = this.dealer.nextActionLabel.toUpperCase();

        // Show current best hand if we have 5+ cards
        const allCards = this.state.allCards;
        let handPreview = '';
        if (allCards.length >= 5) {
            const result = this.evaluator.evaluate(allCards);
            handPreview = `
                <div class="hand-preview">
                    <span class="hand-preview-label">CURRENT BEST:</span>
                    <span class="hand-preview-value">${result.name.toUpperCase()}</span>
                    <span class="hand-preview-pts">${result.points} PTS</span>
                </div>
            `;
        } else if (allCards.length > 0) {
            const result = this.evaluator.evaluate(allCards);
            handPreview = `
                <div class="hand-preview partial">
                    <span class="hand-preview-label">SO FAR:</span>
                    <span class="hand-preview-value">${result.name.toUpperCase()}</span>
                </div>
            `;
        }

        return `
            <div class="phase-panel street-panel">
                <div class="phase-title">${label}</div>
                <div class="phase-subtitle">Total bet: ${status.currentBet} chips</div>
                ${handPreview}

                <div class="bet-total">RAISE: ${this._pendingBet}</div>
                <div class="bet-breakdown">${this._chipBreakdown(this._pendingBet)}</div>

                <div class="chip-denom-rows">
                    ${this._chipDenomRowsHTML(this._pendingBet, status.maxBet)}
                </div>

                <div class="bet-custom">
                    <input type="number"
                           id="raise-input"
                           min="${status.minBet}"
                           max="${status.maxBet}"
                           value="${this._pendingBet}"
                           placeholder="Raise amount">
                    <div class="risk-indicator" id="risk-indicator">
                        ${this._riskBadge(this._pendingBet)}
                    </div>
                </div>

                <div class="phase-actions">
                    <button id="btn-advance-street" class="action-btn primary">${next}</button>
                    <button id="btn-raise" class="action-btn">RAISE</button>
                    <button id="btn-fold" class="action-btn danger">FOLD</button>
                </div>
            </div>
        `;
    }

    // Resolve — show outcome after river
    _phaseResolve() {
        const resolution = this.dome.resolveHand();
        this._lastResolution = resolution; // Store for cashout phase reference

        const won    = resolution.beatDome;
        const cls    = won ? 'result-win' : 'result-loss';
        const banner = won ? '▲ BEAT THE DOME' : '▼ DOME WINS';

        return `
            <div class="phase-panel resolve-panel ${cls}">
                <div class="result-banner">${banner}</div>
                <div class="result-hand">${resolution.description.toUpperCase()}</div>
                <div class="result-meta">
                    <span>${resolution.points} PTS</span>
                    <span>vs</span>
                    <span>THRESHOLD: ${resolution.threshold} PTS</span>
                </div>
                <div class="result-chips ${won ? 'chips-won' : 'chips-lost'}">
                    ${won ? `+${resolution.chipsWon}` : `-${resolution.chipsLost}`} CHIPS
                </div>
                <div class="result-flavor">"${resolution.flavor}"</div>
                <div class="phase-actions">
                    <button id="btn-to-cashout" class="action-btn primary">CONTINUE</button>
                </div>
            </div>
        `;
    }

    // Cash-out — bank some chips, escape, or continue
    _phaseCashOut() {
        const cashOuts    = this.betting.suggestedCashOuts;
        const withdrawals = this.betting.suggestedWithdrawals;
        const chips       = this.state.chips;
        const bank        = this.state.bank;
        const preview     = this.dome.antePreview(1)[0];
        const danger      = chips <= preview.ante;

        return `
            <div class="phase-panel cashout-panel">
                <div class="phase-title">CASH OUT?</div>
                <div class="cashout-totals">
                    <div class="cashout-stat">
                        <span class="cashout-stat-label">CHIPS</span>
                        <span class="cashout-stat-value">${chips}</span>
                    </div>
                    <div class="cashout-stat">
                        <span class="cashout-stat-label">BANK</span>
                        <span class="cashout-stat-value">${bank}</span>
                    </div>
                </div>

                ${danger ? `<div class="ante-danger">⚠ NEXT ANTE IS ${preview.ante} — LOW CHIPS</div>` : ''}

                <div class="cashout-label">MOVE CHIPS TO BANK:</div>
                <div class="bet-quick-picks">
                    ${cashOuts.map(c => `
                        <button class="bet-quick-btn cashout-quick-btn" data-amount="${c}">${c}</button>
                    `).join('')}
                </div>

                <div class="bet-custom">
                    <input type="number"
                           id="cashout-input"
                           min="0"
                           max="${chips}"
                           value="0"
                           placeholder="Amount to bank">
                </div>

                <div class="phase-actions">
                    <button id="btn-cashout-partial" class="action-btn">BANK AMOUNT</button>
                    <button id="btn-cashout-all" class="action-btn">BANK ALL</button>
                </div>

                ${bank > 0 ? `
                <div class="withdraw-divider">— OR DIG INTO SAVINGS —</div>
                <div class="withdraw-label">PULL CHIPS FROM BANK:</div>
                <div class="withdraw-quick-picks">
                    ${withdrawals.map(w => `
                        <button class="bet-quick-btn withdraw-quick-btn" data-amount="${w}">${w}</button>
                    `).join('')}
                </div>
                <div class="bet-custom">
                    <input type="number"
                           id="withdraw-input"
                           min="0"
                           max="${bank}"
                           value="0"
                           placeholder="Amount to withdraw">
                </div>
                <div class="phase-actions">
                    <button id="btn-withdraw-partial" class="action-btn">WITHDRAW AMOUNT</button>
                    <button id="btn-withdraw-all" class="action-btn">WITHDRAW ALL</button>
                </div>
                ` : ''}

                <div class="phase-actions" style="margin-top:16px">
                    <button id="btn-next-round" class="action-btn primary" ${chips <= 0 && bank <= 0 ? 'disabled' : ''}>NEXT ROUND ►</button>
                    <button id="btn-escape" class="action-btn escape-btn">ESCAPE DOME</button>
                </div>
            </div>
        `;
    }

    // ── Hand status bar ──────────────────────────────────────
    // Small persistent bar showing threshold vs current best

    _renderHandStatus() {
        const el = document.getElementById('ui-hand-status');
        if (!el) return;

        const allCards = this.state.allCards;
        if (allCards.length === 0) {
            el.innerHTML = `
                <span class="hand-status-item">DOME THRESHOLD: ${this.state.currentDomeThreshold} PTS</span>
            `;
            return;
        }

        const result = this.evaluator.evaluate(allCards);
        const beating = result.points >= this.state.currentDomeThreshold;

        el.innerHTML = `
            <span class="hand-status-item">BEST: <strong>${result.name.toUpperCase()}</strong> (${result.points} PTS)</span>
            <span class="hand-status-sep">|</span>
            <span class="hand-status-item">THRESHOLD: ${this.state.currentDomeThreshold} PTS</span>
            <span class="hand-status-badge ${beating ? 'beating' : 'losing'}">${beating ? '▲ BEATING' : '▼ LOSING'}</span>
        `;
    }

    // ── Phase event listeners ────────────────────────────────

    _attachPhaseListeners() {
        // ── Idle ─────────────────────────────────────────────
        this._on('btn-start-round', 'click', () => this._startRound());
        this._on('btn-escape-pre',  'click', () => this._handleEscape());

        this._onAll('.withdraw-quick-btn', 'click', (e) => {
            const input = document.getElementById('withdraw-input');
            if (input) input.value = e.currentTarget.dataset.amount;
        });

        this._on('btn-withdraw-partial', 'click', () => {
            const input  = document.getElementById('withdraw-input');
            const amount = parseInt(input?.value) || 0;
            const result = this.betting.withdrawFromBank(amount);
            if (!result.ok) { this._showError(result.error); return; }
            this._startRound();
        });

        // ── Betting ──────────────────────────────────────────
        this._on('bet-input', 'input', (e) => {
            this._pendingBet = parseInt(e.target.value) || this.betting.minBet;
            this._updateBetButton();
            this._updateRiskIndicator();
        });

        this._onAll('.bet-quick-btn', 'click', (e) => {
            this._pendingBet = parseInt(e.currentTarget.dataset.amount);
            const input = document.getElementById('bet-input') ||
                          document.getElementById('raise-input');
            if (input) input.value = this._pendingBet;
            this._updateBetButton();
            this._updateRiskIndicator();
            // Update selected state
            document.querySelectorAll('.bet-quick-btn').forEach(b =>
                b.classList.toggle('selected', parseInt(b.dataset.amount) === this._pendingBet)
            );
        });

        // ── Chip denomination +/- buttons ────────────────────
        this._onAll('.chip-plus', 'click', (e) => {
            const denom = parseInt(e.currentTarget.dataset.denom);
            this._pendingBet = Math.min(this._pendingBet + denom, this.betting.maxBet);
            this._syncBetInput();
            this._updateBetButton();
            this._updateRiskIndicator();
            this._renderPhasePanel();
        });

        this._onAll('.chip-minus', 'click', (e) => {
            const denom = parseInt(e.currentTarget.dataset.denom);
            this._pendingBet = Math.max(this._pendingBet - denom, 0);
            this._syncBetInput();
            this._updateBetButton();
            this._updateRiskIndicator();
            this._renderPhasePanel();
        });

        this._on('btn-place-bet', 'click', () => {
            const result = this.betting.placeBet(this._pendingBet);
            if (!result.ok) { this._showError(result.error); return; }
            this.dealer.advanceStreet(); // Deal flop
            this.render();
        });

        this._on('btn-withdraw-bet', 'click', () => {
            const input  = document.getElementById('withdraw-input');
            const amount = parseInt(input?.value) || 0;
            const result = this.betting.withdrawFromBank(amount);
            if (!result.ok) { this._showError(result.error); return; }
            // Refresh bet constraints and re-render the panel
            this._pendingBet = 0;
            this.render();
        });

        this._on('btn-check-preflop', 'click', () => {
            this.betting.check();
            this.dealer.advanceStreet(); // Deal flop for free
            this.render();
        });

        // ── Street (flop/turn/river) ──────────────────────────
        this._on('raise-input', 'input', (e) => {
            this._pendingBet = parseInt(e.target.value) || this.betting.minBet;
            this._updateRiskIndicator();
        });

        this._on('btn-advance-street', 'click', () => {
            this.dealer.advanceStreet();
            this.render();
        });

        this._on('btn-raise', 'click', () => {
            const result = this.betting.raise(this._pendingBet);
            if (!result.ok) { this._showError(result.error); return; }
            this.dealer.advanceStreet();
            this.render();
        });

        this._on('btn-fold', 'click', () => {
            this.betting.fold();
            this.dealer.fold();
            this.render();
        });

        // ── Resolve ──────────────────────────────────────────
        this._on('btn-to-cashout', 'click', () => {
            this.state.phase = 'cashout';
            this.render();
        });

        // ── Cash-out ─────────────────────────────────────────
        this._onAll('.cashout-quick-btn', 'click', (e) => {
            const input = document.getElementById('cashout-input');
            if (input) input.value = e.currentTarget.dataset.amount;
        });

        this._on('btn-cashout-partial', 'click', () => {
            const input  = document.getElementById('cashout-input');
            const amount = parseInt(input?.value) || 0;
            const result = this.betting.cashOutPartial(amount);
            if (!result.ok) { this._showError(result.error); return; }
            this.render();
        });

        this._on('btn-cashout-all', 'click', () => {
            this.betting.cashOutAll();
            this.render();
        });

        this._onAll('.withdraw-quick-btn', 'click', (e) => {
            const input = document.getElementById('withdraw-input');
            if (input) input.value = e.currentTarget.dataset.amount;
        });

        this._on('btn-withdraw-partial', 'click', () => {
            const input  = document.getElementById('withdraw-input');
            const amount = parseInt(input?.value) || 0;
            const result = this.betting.withdrawFromBank(amount);
            if (!result.ok) { this._showError(result.error); return; }
            this.render();
        });

        this._on('btn-withdraw-all', 'click', () => {
            const result = this.betting.withdrawFromBank(this.state.bank);
            if (!result.ok) { this._showError(result.error); return; }
            this.render();
        });

        this._on('btn-next-round', 'click', () => {
            const result = this.dome.startNextRound();
            if (result.bust) { this._handleSessionOver(result); return; }
            this.render();
        });

        this._on('btn-escape', 'click', () => this._handleEscape());
    }

    // ── Session over ─────────────────────────────────────────

    _handleSessionOver(result) {
        const panel = document.getElementById('ui-phase-panel');
        if (!panel) return;

        const escaped    = result.escaped || this.state.escaped;
        const banner     = escaped ? 'YOU ESCAPED THE DOME' : 'THE DOME CLAIMS YOU';
        const cls        = escaped ? 'result-win' : 'result-loss';
        const finalScore = this.state.bank;
        const isHigh     = this.scoring.isHighScore(finalScore);

        panel.innerHTML = `
            <div class="phase-panel session-over-panel ${cls}">
                <div class="result-banner">${banner}</div>
                <div class="session-stats">
                    <div class="session-stat">
                        <span class="session-stat-label">FINAL SCORE</span>
                        <span class="session-stat-value">${finalScore.toLocaleString()}</span>
                    </div>
                    <div class="session-stat">
                        <span class="session-stat-label">ROUNDS SURVIVED</span>
                        <span class="session-stat-value">${this.state.round}</span>
                    </div>
                    <div class="session-stat">
                        <span class="session-stat-label">DEPTH REACHED</span>
                        <span class="session-stat-value">${this.state.currentDepthLabel.label.toUpperCase()}</span>
                    </div>
                </div>
                <div class="result-flavor">"${result.flavor || ''}"</div>

                ${isHigh ? `
                <div class="high-score-entry">
                    <div class="phase-subtitle">★ NEW HIGH SCORE — ENTER INITIALS:</div>
                    <div class="bet-custom">
                        <input type="text" id="initials-input" maxlength="3"
                               placeholder="AAA"
                               style="text-transform:uppercase; width:100px; font-size:18px; text-align:center;">
                        <button id="btn-submit-score" class="action-btn primary">SUBMIT</button>
                    </div>
                </div>` : ''}

                <div class="phase-actions">
                    <button id="btn-new-session" class="action-btn primary" ${isHigh ? 'style="display:none"' : ''}>PLAY AGAIN</button>
                    <button id="btn-view-scores" class="action-btn">HIGH SCORES</button>
                </div>
            </div>
        `;

        // High score submission
        if (isHigh) {
            const submitScore = async () => {
                const input    = document.getElementById('initials-input');
                const initials = (input?.value || 'AAA').toUpperCase();
                await this.scoring.submitScore(initials, finalScore, this.state.round, escaped);
                this.scoring.displayHighScores();
                // Swap submit area for play again
                document.querySelector('.high-score-entry').innerHTML =
                    `<div class="phase-subtitle" style="color:var(--yellow)">★ SCORE SAVED!</div>`;
                const playAgain = document.getElementById('btn-new-session');
                if (playAgain) playAgain.style.display = '';
            };

            this._on('btn-submit-score', 'click', submitScore);

            const input = document.getElementById('initials-input');
            if (input) {
                input.focus();
                input.addEventListener('input', e => {
                    e.target.value = e.target.value.toUpperCase().replace(/[^A-Z]/g, '');
                });
                input.addEventListener('keydown', e => {
                    if (e.key === 'Enter') submitScore();
                });
            }
        }

        this._on('btn-new-session', 'click', () => this._startSession());
        this._on('btn-view-scores', 'click', () => {
            this.scoring.displayHighScores();
            document.getElementById('highScoresModal').classList.add('active');
        });
    }

    _handleEscape() {
        const result = this.dome.escape();
        this._handleSessionOver(result);
    }

    // ── DOM helpers ──────────────────────────────────────────

    _set(id, value) {
        const el = document.getElementById(id);
        if (el) el.textContent = value;
    }

    _on(id, event, handler) {
        const el = document.getElementById(id);
        if (el) el.addEventListener(event, handler);
    }

    _onAll(selector, event, handler) {
        document.querySelectorAll(selector).forEach(el => {
            el.addEventListener(event, handler);
        });
    }

    _updateBetButton() {
        const betBtn = document.getElementById('btn-place-bet');
        if (betBtn) betBtn.textContent = `BET ${this._pendingBet}`;
        const raiseBtn = document.getElementById('btn-raise');
        if (raiseBtn) raiseBtn.textContent = `RAISE ${this._pendingBet}`;
    }

    _syncBetInput() {
        const input = document.getElementById('bet-input') ||
                      document.getElementById('raise-input');
        if (input) input.value = this._pendingBet;
    }

    _updateRiskIndicator() {
        const el = document.getElementById('risk-indicator');
        if (el) el.innerHTML = this._riskBadge(this._pendingBet);
    }

    _riskBadge(amount) {
        if (!amount) return '';
        const risk = this.betting.riskLevel(amount);
        return `<span class="risk-badge risk-${risk.level}">${risk.label}</span>`;
    }

    // ── Chip denomination helpers ────────────────────────────

    _chipBreakdown(amount) {
        if (!amount) return '';
        const stacks = breakIntoStacks(amount);
        return stacks.map(s => {
            const label = s.denom.label;
            return s.count > 1 ? `${label}×${s.count}` : `${label}`;
        }).join(' + ');
    }

    _chipDenomRowsHTML(currentBet, maxBet) {
        return DENOMS.map(d => {
            const count = Math.floor(currentBet / d.value);
            const canAdd = currentBet + d.value <= maxBet;
            const canRemove = count > 0;
            return `
                <div class="chip-denom-row">
                    <span class="chip-denom-label" style="color:${d.face}">${d.label}</span>
                    <button class="chip-btn chip-minus${canRemove ? '' : ' disabled'}" data-denom="${d.value}">−</button>
                    <span class="chip-denom-count">${count}</span>
                    <button class="chip-btn chip-plus${canAdd ? '' : ' disabled'}" data-denom="${d.value}">+</button>
                </div>
            `;
        }).join('');
    }

    _showError(message) {
        // Flash error in phase panel subtitle or a toast
        const existing = document.getElementById('ui-error-toast');
        if (existing) existing.remove();

        const toast = document.createElement('div');
        toast.id = 'ui-error-toast';
        toast.className = 'error-toast';
        toast.textContent = message;
        document.getElementById('ui-phase-panel')?.appendChild(toast);
        setTimeout(() => toast.remove(), 2500);
    }

    _chipDisplay(amount) {
        return amount.toLocaleString();
    }

    // ── Build game screen HTML ───────────────────────────────
    // Replaces the old poker solitaire game screen with the new layout

    _buildGameScreenHTML() {
        const gameScreen = document.getElementById('gameScreen');
        if (!gameScreen) return;

        gameScreen.innerHTML = `
            <!-- Top bar -->
            <div class="header">
                <h1>TEXAS HOLD'EM LAVA DOME</h1>
                <div class="stats">
                    <div class="score-container">
                        <div class="score-label">Chips</div>
                        <div class="score" id="ui-chips">500</div>
                    </div>
                    <div class="score-container">
                        <div class="score-label">Bank</div>
                        <div class="score" id="ui-bank">0</div>
                    </div>
                    <div class="score-container">
                        <div class="score-label">Round</div>
                        <div class="score" id="ui-round">1</div>
                    </div>
                    <div class="score-container">
                        <div class="score-label">Ante</div>
                        <div class="score" id="ui-ante">10</div>
                    </div>
                </div>
            </div>

            <!-- Controls -->
            <div class="controls">
                <button id="newGame">New Game</button>
                <button id="toggleInstructions">How to Play</button>
                <button id="toggleHighScores">High Scores</button>
                <button id="toggleCredits">Credits</button>
            </div>

            <!-- Depth bar -->
            <div class="depth-bar">
                <span class="depth-label" id="ui-depth-label">LITHOSPHERE</span>
                <span class="ante-warning" id="ui-ante-warning" style="display:none"></span>
            </div>

            <!-- Game area -->
            <div class="game-area">

                <!-- Left: community cards + phase panel (raised up) -->
                <div class="game-area-left">
                    <!-- Community cards -->
                    <div class="community-section">
                        <div class="section-label">COMMUNITY CARDS</div>
                        <div class="community-cards" id="ui-community-cards"></div>
                    </div>

                    <!-- Phase panel — changes each phase -->
                    <div id="ui-phase-panel"></div>
                </div>

                <!-- Right: chip stack + your hand (stacked) -->
                <div class="chip-display-area">
                    <div class="section-label">CHIP STACK</div>
                    <div id="chipDisplay" class="chip-display"></div>
                    <div class="chip-legend" id="chipLegend"></div>

                    <div class="hole-section">
                        <div class="section-label">YOUR HAND</div>
                        <div class="hole-cards" id="ui-hole-cards"></div>
                    </div>

                    <!-- Hand status bar -->
                    <div class="hand-status-bar" id="ui-hand-status"></div>
                </div>

            </div>
        `;

        // Re-attach top-level game control listeners
        this._attachGameControlListeners();
    }

    // ── Top-level listeners ──────────────────────────────────

    _attachTopLevelListeners() {
        // Start screen
        this._on('startGameBtn', 'click', () => this.showGameScreen());
        this._on('viewRulesBtn', 'click', () => {
            document.getElementById('instructionsModal').classList.add('active');
        });
        this._on('viewScoresBtn', 'click', () => {
            this.scoring.displayHighScores();
            document.getElementById('highScoresModal').classList.add('active');
        });
        this._on('viewCreditsBtn', 'click', () => {
            document.getElementById('creditsModal').classList.add('active');
        });
        this._on('toggleHighScores', 'click', () => {
            this.scoring.displayHighScores();
            document.getElementById('highScoresModal').classList.add('active');
        });

        // Modals
        this._on('closeInstructions', 'click', () => {
            document.getElementById('instructionsModal').classList.remove('active');
        });
        this._on('closeHighScores', 'click', () => {
            document.getElementById('highScoresModal').classList.remove('active');
        });
        this._on('closeCredits', 'click', () => {
            document.getElementById('creditsModal').classList.remove('active');
        });

        document.querySelectorAll('.instructions-modal').forEach(modal => {
            modal.addEventListener('click', e => {
                if (e.target === modal) modal.classList.remove('active');
            });
        });
    }

    _attachGameControlListeners() {
        this._on('newGame', 'click', () => this._startSession());
        this._on('toggleInstructions', 'click', () => {
            document.getElementById('instructionsModal').classList.add('active');
        });
        this._on('toggleHighScores', 'click', () => {
            this.scoring.displayHighScores();
            document.getElementById('highScoresModal').classList.add('active');
        });
        this._on('toggleCredits', 'click', () => {
            document.getElementById('creditsModal').classList.add('active');
        });
    }
}
