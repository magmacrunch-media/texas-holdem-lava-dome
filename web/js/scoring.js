// scoring.js - Texas Hold'Em Lava Dome | MagmaCrunch Media © 2024
// High score persistence via MAGMA//OPS backend (ScoreClient)

class Scoring {
    constructor(state) {
        this.state     = state;
        this.isLoading = false;
        this.isSaving  = false;
    }

    // ── Load ─────────────────────────────────────────────────

    async loadHighScores() {
        if (this.isLoading) return;
        this.isLoading = true;

        try {
            this.state.highScores = await scoreClient.load('solitaire-thld');
        } catch (err) {
            console.warn('Could not load high scores:', err);
            this.state.highScores = [];
        } finally {
            this.isLoading = false;
        }
    }

    // ── Save ─────────────────────────────────────────────────

    async saveHighScores() {
        if (this.isSaving) return;
        this.isSaving = true;

        try {
            for (const entry of this.state.highScores) {
                await scoreClient.save('solitaire-thld', entry.initials, entry.totalScore, {
                    rounds:   entry.rounds,
                    escaped:  entry.escaped,
                    date:     entry.date
                });
            }
        } catch (err) {
            console.warn('Could not save high scores:', err);
        } finally {
            this.isSaving = false;
        }
    }

    // ── Check & submit ───────────────────────────────────────

    isHighScore(finalScore) {
        if (finalScore <= 0) return false;
        if (this.state.highScores.length < 10) return true;
        return finalScore > this.state.highScores[this.state.highScores.length - 1].totalScore;
    }

    async submitScore(initials, finalScore, rounds, escaped) {
        // Clear old "new" flags
        this.state.highScores.forEach(s => s.isNew = false);

        // Add new entry
        this.state.highScores.push({
            initials:   initials.toUpperCase().slice(0, 3) || 'AAA',
            totalScore: finalScore,
            rounds,
            escaped,
            isNew:      true,
            date:       new Date().toISOString().slice(0, 10)
        });

        // Sort descending, keep top 10
        this.state.highScores.sort((a, b) => b.totalScore - a.totalScore);
        this.state.highScores = this.state.highScores.slice(0, 10);

        await this.saveHighScores();
    }

    // ── Render ───────────────────────────────────────────────

    displayHighScores() {
        const el = document.getElementById('highScoresList');
        if (!el) return;

        el.innerHTML = '';

        if (!this.state.highScores.length) {
            el.innerHTML = `
                <div style="text-align:center; padding: 20px;">
                    <span style="font-family: var(--font-pixel); font-size: 7px;
                                 color: var(--orange-glow);">
                        NO SCORES YET — BE THE FIRST
                    </span>
                </div>`;
            return;
        }

        const fragment = document.createDocumentFragment();

        this.state.highScores.slice(0, 10).forEach((entry, i) => {
            const row = document.createElement('div');
            row.className = 'score-entry' + (entry.isNew ? ' new-score' : '');
            row.innerHTML = `
                <span class="rank">#${i + 1}</span>
                <span class="initials">${entry.initials}</span>
                <span class="score-value">${(entry.totalScore || 0).toLocaleString()}</span>
            `;
            fragment.appendChild(row);
        });

        el.appendChild(fragment);
    }
}
