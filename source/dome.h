#ifndef DOME_H
#define DOME_H

#include "hand_eval.h"

/* The dome: the money, the escalating ante, and the two ways a session ends.
 *
 * Ported from js/dome.js, js/state.js and js/betting.js in the web build. The
 * tuned numbers come from js/config.js and are not to be adjusted by feel --
 * the ante schedule and the threshold curve are in tension with each other, and
 * that tension is the game.
 *
 * There is no opponent. A hand is scored in hand_eval points and compared
 * against a threshold that climbs every round, while the ante drains the stack
 * whether you play well or not. The pressure is arithmetic, not an AI.
 */

/* Session start. From js/config.js. */
#define STARTING_CHIPS   500
#define MIN_BET           10

/* Threshold = DOME_BASE_THRESHOLD + (round - 1) * DOME_THRESHOLD_SCALE.
   Round 1 lands on exactly 10, which is One Pair; by round 16 it is 85, which
   needs a Flush. High Card is worth 0 points and so can never beat the dome at
   any round -- that is deliberate, not an oversight in the table. */
#define DOME_BASE_THRESHOLD   10
#define DOME_THRESHOLD_SCALE   5

/* Rounds the hand-written ante schedule covers; past it the last value climbs
   by DOME_ANTE_ESCALATION per round, forever. */
#define DOME_ANTE_ROUNDS      15
#define DOME_ANTE_ESCALATION  25

typedef enum {
    DOME_PHASE_IDLE,
    DOME_PHASE_BETTING,
    DOME_PHASE_FLOP,
    DOME_PHASE_TURN,
    DOME_PHASE_RIVER,
    DOME_PHASE_CASHOUT,
    /* Short of the ante with money in the bank. A real state of the round, not
       a UI mode: the ante has not been charged yet and cannot be until the
       player fetches chips, so the round is genuinely waiting on them. */
    DOME_PHASE_SAVINGS
} DomePhase;

typedef struct {
    int chips;          /* at risk */
    int bank;           /* safe, and the final score */
    int round;          /* 0 before the first round starts, then 1-based */
    int ante;           /* actually paid this round, which can be short */
    int current_bet;    /* committed this round, already deducted from chips */

    DomePhase phase;
    int session_over;
    int escaped;        /* walked away rather than went bust */
} Dome;

void dome_init(Dome *d);

/* Schedule lookups. Pure functions of the round number, so the UI can show what
   the next ante will be without advancing anything. Round numbers below 1 are
   treated as round 1. */
int dome_ante_for_round(int round);
int dome_threshold_for_round(int round);

/* Flavour text for the round's depth, themed after the band's songs. */
const char *dome_depth_label(int round);

/* Advances to the next round. Returns 0 and ends the session when there is
   nothing left to play with -- both chips and bank empty. Note that a player
   with an empty stack but a filled bank is NOT bust: they have a score, and the
   session ends by the ante taking the last of it rather than here. */
int dome_start_round(Dome *d);

/* Charges the round's ante. A player who cannot cover it pays what they have,
   which empties the stack and ends the session -- the dome does not extend
   credit. Returns 1 if the round can proceed, 0 if the session just ended. */
int dome_charge_ante(Dome *d);

/* Bet sizing. The maximum is the whole stack: this game lets you go all-in.
   (js/config.js declares a MAX_BET_MULTIPLIER of 10 which nothing reads -- the
   behaviour ported here is js/betting.js's, which caps at the stack.) */
int dome_min_bet(const Dome *d);
int dome_max_bet(const Dome *d);

/* Commits chips to the pot, clamped into the legal range. Returns the amount
   actually taken. Deducted immediately, so `chips` always reads as what is
   still yours to lose. */
int dome_place_bet(Dome *d, int amount);

typedef struct {
    HandRank hand;
    int points;
    int threshold;
    int beat_dome;
    int chips_won;      /* winnings on top of the stake returned */
    int chips_lost;     /* the stake, when the dome wins */
    int bust;
} DomeResult;

/* Scores the finished hand against the round's threshold and settles the bet.
   On a win the stake comes back along with the winnings; on a loss it is gone.
   Moves to DOME_PHASE_CASHOUT. */
DomeResult dome_resolve(Dome *d, const HandResult *hand);

/* Payout is the stake times this, in hundredths -- 250 means two and a half
   times. Fractions are real here: a Flush pays 2.5x and Two Pair 1.25x. */
int dome_payout_hundredths(HandRank rank);

/* Moves chips to the bank, where they cannot be lost. Clamped to what is there;
   returns the amount actually banked. */
int dome_cash_out(Dome *d, int amount);

/* The other direction: pulls banked chips back onto the table. Clamped to the
   bank; returns the amount actually withdrawn.
 *
 * This exists for one situation, and without it that situation is a dead end: a
 * stack too small to meet the minimum bet, with a bank that is not empty. The
 * player has chips, they are just in the wrong place, and a betting screen they
 * cannot act on is not a decision. Banking is safe but not permanent -- which is
 * also what stops "bank everything immediately" from being the whole strategy. */
int dome_withdraw_from_bank(Dome *d, int amount);

/* Whether the player is in that dead end and should be offered the way out:
   short of the minimum bet, with something banked to fetch. */
int dome_must_dig_into_savings(const Dome *d);

/* The same dead end one step earlier, and the more important one: short of the
   round's ante, with a bank that could cover it.
 *
 * Without this, banking your whole stack is a trap. You do the safe thing, the
 * next round charges an ante you cannot pay, and the session ends as a bust
 * holding a full bank -- punished for banking, which is the opposite of what
 * the mechanic is for. The ante must not be charged while this is true; give
 * the player the chance to fetch chips first. */
int dome_needs_savings_for_ante(const Dome *d);

/* Ends the session on the player's terms: banks everything left and marks the
   session escaped rather than bust. The score is the same either way -- what
   differs is whether there were chips left to save. */
void dome_escape(Dome *d);

/* Whether escaping is offered: only between rounds, and only with something to
   walk away with. */
int dome_can_escape(const Dome *d);

/* The score a session is worth. The bank, and only the bank -- chips still on
   the table are not yours until you cash them. */
int dome_score(const Dome *d);

#endif
