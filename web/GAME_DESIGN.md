# Texas Hold'Em Lava Dome — Game Design Summary

**Developer:** MagmaCrunch Media  
**Concept:** Solo Texas Hold'Em with escalating stakes, a chip bank system, and a volcanic survival framing

---

## Concept

Texas Hold'Em Lava Dome is a solo card game inspired by Texas Hold'Em poker, built around the irony of playing the world's most social card game completely alone — against the dome itself. It combines the luck and structure of real Hold'Em with a risk/reward cash-out mechanic borrowed from solitaire, creating genuine strategic tension without a human opponent.

---

## Core Loop

1. **The dome charges an ante** — a fixed cost deducted from your chip stack at the start of each round. The ante escalates as rounds progress. Even conservative play eventually becomes unsustainable.
2. **Play a Hold'Em hand** — you receive two hole cards and bet before seeing the flop. The flop, turn, and river are revealed one at a time, with an opportunity to raise or fold at each stage.
3. **Hand resolves against a point threshold** — rather than a human opponent, your hand is evaluated against the dome's current point threshold. Beat it, win chips. Miss it, lose your bet.
4. **Cash out or press your luck** — after each round you choose: move some or all of your chips into your permanent bank (safe), or leave them in play as betting chips for the next round (risky, but potentially more lucrative).
5. **Escape or bust** — the session ends one of two ways: you voluntarily "escape the dome," converting all remaining chips to bank points and ending the session, or you go bust, losing everything. Your final score is your bank total.

---

## Key Mechanics

### The Dome Ante
- Starts small, escalates each round
- Forces action — you cannot survive on passive play forever
- Represents the dome "getting hungrier" the deeper you go
- Round depth markers themed after band songs/albums (e.g. *Lithosphere*, *Penultimate Drop*, *Figure the Shoreline*)

### The Bank
- A permanent, protected score total
- Chips cashed into the bank cannot be lost
- Cashing out too early limits your score ceiling
- Cashing out too late risks losing everything to the dome
- "Knowing when to leave" is a core strategic decision

### Chip Stack
- Your active, at-risk currency
- Used to pay antes and place bets
- Can be replenished by winning hands
- Hitting zero means the dome wins — session over

### Hand Evaluation
- Standard Texas Hold'Em: best 5-card hand from 2 hole cards + 5 community cards
- Full street structure: hole cards → flop → turn → river
- Betting opportunity at each street

---

## Session End Conditions

| Condition | Outcome |
|-----------|---------|
| Voluntary escape | All chips converted to bank; final score recorded |
| Bust (chips = 0) | Session ends; bank total is final score |

---

## File Structure

| File | Purpose |
|------|---------|
| `config.js` | Constants: starting chips, ante schedule, hand scores, dome thresholds |
| `deck.js` | Card and Deck classes (carried over) |
| `state.js` | Game state: chip stack, bank, round number, current hand |
| `dealer.js` | Deck management, dealing hole cards, flop/turn/river |
| `hand-eval.js` | 7-card best-hand evaluator (2 hole + 5 community) |
| `dome.js` | Ante escalation, threshold logic, bust/escape conditions |
| `betting.js` | Bet sizing, raise/fold logic, cash-out mechanic |
| `ui.js` | DOM updates and rendering |
| `main.js` | Initialization and event wiring |

---

## Visual Direction

- SNES-era pixel aesthetic (Press Start 2P font)
- Lava/volcanic color palette (established)
- Card redesign planned — especially face cards, to be themed around the band
- Round depth framed as descending into the lava dome
- Band (Texas Hold'Em Lava Dome) and label (MagmaCrunch Media) integrated into UI copy and flavor text

---

*Based on brainstorming session, February 2026*
