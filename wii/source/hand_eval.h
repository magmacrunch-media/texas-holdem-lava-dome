#ifndef HAND_EVAL_H
#define HAND_EVAL_H

#include "cards.h"

/* Poker hand evaluation, ported from AdCards.HandEvaluator in the web build's
 * shared/adenosine-cards.js.
 *
 * Two numbers come out of an evaluation and they are not the same thing:
 *
 *   rank   orders hands against each other, 0..9. Only used for comparisons.
 *   points is what the dome is beaten with, and the scale is deliberately not
 *          linear -- One Pair 10, Royal Flush 1000. The dome's threshold is
 *          measured in these, so they are game balance, not presentation.
 */

typedef enum {
    HAND_HIGH_CARD,
    HAND_ONE_PAIR,
    HAND_TWO_PAIR,
    HAND_THREE_OF_A_KIND,
    HAND_STRAIGHT,
    HAND_FLUSH,
    HAND_FULL_HOUSE,
    HAND_FOUR_OF_A_KIND,
    HAND_STRAIGHT_FLUSH,
    HAND_ROYAL_FLUSH,
    HAND_CLASS_COUNT
} HandRank;

#define HAND_MAX_CARDS   7   /* 2 hole + 5 community */
#define HAND_TIEBREAK_MAX 5

typedef struct {
    HandRank rank;
    int      points;
    /* Ordered most significant first: the quad rank before its kicker, the trips
       before the pair, the five card values of a flush. Compared element by
       element, which is what settles two hands of the same class. */
    int      tiebreak[HAND_TIEBREAK_MAX];
    int      tiebreak_count;
    int      valid;   /* 0 when fewer than five cards were given */
} HandResult;

/* Points for a hand class. Exposed because the dome's threshold schedule is
   tuned against these and reads them directly. */
int hand_points(HandRank rank);

/* Display name: "Royal Flush", "Two Pair", "High Card". */
const char *hand_name(HandRank rank);

/* Best five-card hand out of `count` cards, 5..7. Below five the result is
   marked invalid rather than guessed at: the web build scores partial hands to
   show the player something during the streets, but nothing is ever *resolved*
   against a partial hand, and a scoreable-looking result for two hole cards is
   an invitation to resolve one by accident. Use hand_eval_partial() to label a
   part-dealt hand for display. */
HandResult hand_eval(const Card *cards, int count);

/* Exactly five cards. hand_eval() is this plus the best-of choice. */
HandResult hand_eval_five(const Card *cards);

/* A label for 2..4 cards, for showing the player what they are holding before
   the board is out. Counts pairs and trips only -- no straights or flushes,
   which are not decidable yet -- and carries points of 0, because a partial
   hand has not beaten anything. */
HandResult hand_eval_partial(const Card *cards, int count);

/* Negative, zero or positive as `a` ranks below, equal to or above `b`. */
int hand_compare(const HandResult *a, const HandResult *b);

#endif
