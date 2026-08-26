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

/* What the next round will cost, and whether the stack is short enough that it
   is the thing to worry about. Ported from js/dome.js's antePreview() and the
   `danger` flag ui.js computes from it.
 *
 * The depth bar already says when the ante is about to rise. This is the
 * sharper question and the one the cash-out decision actually turns on: not
 * "is it going up" but "can I still pay it". */
int dome_next_ante(const Dome *d);
int dome_ante_danger(const Dome *d);

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

/* Whether there is anything left to raise with. A player already all-in has a
   hand to watch and no decisions left in it. */
int dome_can_raise(const Dome *d);

/* Commits chips to the pot, clamped into the legal range. Returns the amount
   actually taken. Deducted immediately, so `chips` always reads as what is
   still yours to lose.
 *
 * This is also the raise: it adds to `current_bet` rather than replacing it, so
 * a stake built up across four streets is one number by the showdown and the
 * payout multiplier applies to the whole of it. js/betting.js splits placeBet()
 * from raise() and they do the same thing; one function here says so. */
int dome_place_bet(Dome *d, int amount);

/* How exposed a bet of `amount` leaves the player, as the web build's risk
   badge reads it (js/betting.js riskLevel()). Kept in whole percent rather than
   a float for the same reason the payouts are in hundredths: money and the
   warnings about it should not depend on this console's floating point. */
typedef enum {
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH,
    RISK_ALL_IN
} RiskLevel;

RiskLevel   dome_risk_level(const Dome *d, int amount);
const char *dome_risk_label(RiskLevel r);

typedef struct {
    HandRank hand;
    int points;
    int threshold;
    int beat_dome;
    int chips_won;      /* winnings on top of the stake returned */
    int chips_lost;     /* the stake, when the dome wins */
    int bust;
    int folded;         /* walked away from the hand rather than lost it */

    /* Chosen once, when the hand settles -- not when it is drawn. The result
       panel is redrawn every frame from this struct, so a quip picked at draw
       time would change on every one of them. */
    const char *flavor;
} DomeResult;

/* Scores the finished hand against the round's threshold and settles the bet.
   On a win the stake comes back along with the winnings; on a loss it is gone.
   Moves to DOME_PHASE_CASHOUT. */
DomeResult dome_resolve(Dome *d, const HandResult *hand, Rng *rng);

/* Gives up the hand. The stake was taken when it was placed, so folding is
   simply not getting it back -- there is no extra penalty, and none is needed
   when the ante is already climbing.
 *
 * The hand is never consulted, which is the point: folding is the decision you
 * make when the board has missed you and the next street will not save it. */
DomeResult dome_fold(Dome *d, Rng *rng);

/* Payout is the stake times this, in hundredths -- 250 means two and a half
   times. Fractions are real here: a Flush pays 2.5x and Two Pair 1.25x. */
int dome_payout_hundredths(HandRank rank);

/* The band-themed quips from js/config.js, picked uniformly at random.
 *
 * They take the game's own Rng rather than rand() so that a session stays
 * reproducible from its seed, which is already true of every shuffle.
 *
 * FLAVOR_BUST covers a lost hand as well as a real bust, exactly as the web
 * build uses it -- dome.js reaches for the same array in resolveHand() and in
 * _bust(). With no sound and no opponent, this text is most of what the dome
 * has to say for itself, so it is quoted rather than rewritten. */
const char *dome_flavor_win(Rng *rng);
const char *dome_flavor_bust(Rng *rng);
const char *dome_flavor_escape(Rng *rng);

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
   walk away with. Between rounds is both DOME_PHASE_CASHOUT and the
   DOME_PHASE_IDLE that follows it -- they are one decision wearing two phase
   names, and banking a few chips moves the player from the first to the second
   without changing what is on offer. */
int dome_can_escape(const Dome *d);

/* The score a session is worth. The bank, and only the bank -- chips still on
   the table are not yours until you cash them. */
int dome_score(const Dome *d);

#endif
