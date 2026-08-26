# 🔥 Texas Hold'Em Lava Dome

**Developer:** MagmaCrunch Media  
**Based on:** The post space-rock band *Texas Hold'Em Lava Dome*  
**Label/Publisher:** MagmaCrunch Media © 2024

---

## Concept

Texas Hold'Em Lava Dome is a solo card game inspired by Texas Hold'Em poker — built around the deliberate irony of playing the world's most social card game completely alone, against the dome itself. It combines the luck and structure of real Hold'Em with a risk/reward cash-out mechanic, creating genuine strategic tension without a human opponent.

The game is themed after the band *Texas Hold'Em Lava Dome* and their catalog, with round depth markers, flavor text, and UI copy drawn directly from album and song titles across *Martial Law in Garrison Oaks* (2018), *Hazardous Metals in Ambient Air* (2018), and *Pompous Fanfare for All Occasions* (2019).

---

## How to Play

### Session Structure
You start with **500 chips**. Each session consists of as many rounds as you can survive.

### Each Round
1. **Ante** — the dome charges an ante from your chip stack before cards are dealt. The ante escalates as rounds progress.
2. **Hole Cards** — you receive two private cards. Place a bet (or check for free) before seeing community cards.
3. **Flop** — three community cards are revealed. You may raise or advance.
4. **Turn** — a fourth community card. Raise or advance.
5. **River** — the fifth and final community card. Raise or advance to resolution.
6. **Resolution** — your best 5-card hand (from 2 hole + 5 community) is evaluated against the dome's point threshold. Beat it: win chips. Miss it: lose your bet.
7. **Cash Out** — after each round, move some or all chips into your permanent **bank** (safe), or leave them at risk for the next round.

### Ending a Session
- **Escape** — voluntarily end the session at any time during the cash-out phase. All remaining chips convert to bank. Your final score is your bank total.
- **Bust** — chip stack hits zero. Session ends immediately. Bank total is your final score.

### The Bank
Your bank is permanent and protected — chips in the bank cannot be lost. Knowing when to cash out vs. press your luck is the core strategic decision of the game.

---

## Scoring

Hand point values used for dome threshold comparisons and future high score tracking:

| Hand | Points |
|------|--------|
| Royal Flush | 1000 |
| Straight Flush | 500 |
| Four of a Kind | 250 |
| Full House | 150 |
| Flush | 100 |
| Straight | 75 |
| Three of a Kind | 50 |
| Two Pair | 25 |
| One Pair | 10 |
| High Card | 0 |

### Payout Multipliers
Winning hands pay out a multiple of your bet:

| Hand | Multiplier |
|------|-----------|
| Royal Flush | 10× |
| Straight Flush | 6× |
| Four of a Kind | 4× |
| Full House | 3× |
| Flush | 2.5× |
| Straight | 2× |
| Three of a Kind | 1.5× |
| Two Pair | 1.25× |
| One Pair | 1× |
| High Card | 0 (cannot beat the dome) |

### Dome Ante Schedule
The ante escalates through round depth markers themed after band songs:

| Round | Ante | Depth |
|-------|------|-------|
| 1–2 | 10 | Lithosphere |
| 3–4 | 20 | Contemplate the Plate Tectonic |
| 5–6 | 30 | Figure the Shoreline |
| 7–8 | 50 | Penultimate Drop |
| 9–10 | 75 | Pendant Stop |
| 11 | 100 | Hazardous Metals in Ambient Air |
| 13 | 150 | I Would Go Up to the Hot Lava |
| 14 | 200 | Millstone, 2063 |
| 15+ | 250+ | All All & All |

---

## Technical Architecture

### File Structure
```
texas-holdem-lava-dome/
├── index.html
├── css/
│   ├── base.css            # CSS variables, reset, pixel button system, scanlines
│   ├── start-screen.css    # Start screen layout and animations
│   ├── game-layout.css     # Game area layout + all new Hold'Em UI elements
│   ├── cards.css           # Card rendering styles
│   ├── modals.css          # Modal windows (instructions, high scores)
│   └── responsive.css      # Mobile/tablet breakpoints
└── js/
    ├── config.js           # All constants: chips, antes, hand scores, flavor text
    ├── deck.js             # Card and Deck classes (carry-over, solid)
    ├── state.js            # GameState class — session and round state
    ├── dealer.js           # Deck management, dealing streets
    ├── hand-eval.js        # 7-card best-hand evaluator
    ├── dome.js             # Ante charging, hand resolution, bust/escape
    ├── betting.js          # Bet sizing, raises, cash-out logic
    ├── ui.js               # All DOM rendering and event handling
    └── main.js             # Instantiates all classes and boots the game
```

### Class Responsibilities

**`GameState`** (`state.js`)  
Pure data. Session-level state (chips, bank, round), round-level state (phase, current bet, hole/community cards, best hand). Computed getters for current ante, dome threshold, and depth label. No logic — just state and safe chip operations.

**`Dealer`** (`dealer.js`)  
Manages the deck through a Hold'Em hand. `newRound()` resets card state, `dealHoleCards()` / `dealFlop()` / `dealTurn()` / `dealRiver()` advance streets with burn cards. `advanceStreet()` convenience wrapper. `fold()` ends the hand early.

**`HandEvaluator`** (`hand-eval.js`)  
Generates all C(n,5) 5-card combinations from up to 7 cards and returns the best hand. Handles Royal Flush through High Card with proper tiebreaker arrays for comparing equal-rank hands. Correctly handles wheel straight (A-2-3-4-5). `_evaluatePartial()` handles pre-river state gracefully.

**`Dome`** (`dome.js`)  
The antagonist. `chargeAnte()` deducts the ante and detects bust. `resolveHand()` evaluates the player's best hand, compares it to the dome threshold, and applies chip win/loss. `escape()` ends the session voluntarily. `antePreview()` gives the UI upcoming ante values for warnings.

**`Betting`** (`betting.js`)  
Bet validation and math. `placeBet()` / `raise()` / `check()` / `fold()`. `suggestedBets` and `suggestedCashOuts` provide quick-pick amounts. `riskLevel()` returns a label and color for the UI danger indicator.

**`UI`** (`ui.js`)  
Builds the game screen HTML, manages the phase state machine, and handles all DOM events. `_renderPhasePanel()` swaps the entire interactive panel based on `state.phase`. The six phase panels are: `idle`, `betting`, `flop/turn/river`, `resolve`, `cashout`, `session-over`.

---

## Visual Design

### Aesthetic
SNES-era retro pixel aesthetic throughout. Press Start 2P as the primary font, VT323 available for secondary use. Chunky pixel borders with light top-left / dark bottom-right edges (classic SNES raised button system). CRT scanlines via a `position: fixed` `body::before` pseudo-element for GPU-composited performance.

### Color Palette
All colors defined as CSS variables in `base.css`:

| Variable | Value | Use |
|----------|-------|-----|
| `--black` | `#0a0000` | Backgrounds, voids |
| `--dark-red` | `#1c0000` | Container background |
| `--deep-red` | `#3b0000` | Dark borders |
| `--lava-dark` | `#6b0000` | Panels, button base |
| `--lava-bright` | `#cc2200` | Accents, danger |
| `--orange` | `#dd4400` | Borders, dividers |
| `--orange-hot` | `#ff5500` | Button highlights |
| `--orange-glow` | `#ff7700` | Labels, secondary text |
| `--yellow` | `#ffcc00` | Primary text, scores |
| `--white` | `#fff8f0` | Card face background |

### Performance Notes
- No animations on `body` — avoids full-page repaints
- Scanlines on `body::before` with `position: fixed` — compositor layer, painted once
- `background-attachment: scroll` throughout — fixed attachment disables tile caching

---

## Development Status

### ✅ Complete
- Full Texas Hold'Em round structure (hole cards → flop → turn → river)
- 7-card best-hand evaluator with tiebreakers and wheel straight
- Dome ante escalation with band-themed depth markers
- Chip stack and bank system with cash-out mechanic
- Bet sizing with quick-picks, custom input, and risk indicator
- Escape and bust session-end conditions
- Flavor text drawn from band song titles
- SNES retro pixel visual design
- Responsive layout (1100px, 860px, 480px breakpoints)
- Start screen with MagmaCrunch Media branding

### 🚧 Planned / In Progress
- [x] High score persistence — ScoreClient integrated (MAGMA//OPS backend)
- [ ] Card redesign — especially face cards (J, Q, K) themed to the band
- [ ] Sound effects and/or background music
- [ ] Resolve → cashout phase transition polish
- [ ] Responsive layout testing on new Hold'Em UI
- [ ] Multiplayer exploration (longer-term)
- [ ] Potential difficulty settings (ante speed, starting chips)

### 📋 Known Issues
- Resolve panel calls `dome.resolveHand()` on render — should not be called twice
- Old poker solitaire HTML elements (`pokerGrid`, `rowScores`, etc.) still present in `index.html` modals but unused

---

## Band Reference

*Texas Hold'Em Lava Dome* discography used for in-game theming:

**Martial Law in Garrison Oaks** (2018) — *What happened to you in all the confusion*, *Bus full of time-traveling twenty-somethings*, *Pendant Stop*, *Penultimate Drop*, *Figure the Shoreline*, *Lithosphere*, *List of birds by flight height*, *I keep my cards close to my heart*, *Millstone 2063*, *Contemplate the Plate Tectonic*, *All All & All*

**Hazardous Metals in Ambient Air** (2018) — *Purposes*, *Devastate the Environment*, *I would go up to the hot lava and I would climb mountains*, *Haunter*, *Intent*

**Pompous Fanfare for All Occasions** (2019) — *This has always been true*, *Film school*, *Garden vines*, *Ocean of storms*, *Maybe the instruments failed and maybe they didn't*, *Hypnopompia*, *Secret conference rooms*, *Red candy icicles*

Also: *Hello World, Love Space* · *Texas Toast Magma Crunch*

---

*Last updated: February 2026*  
*MagmaCrunch Media — All rights reserved*
