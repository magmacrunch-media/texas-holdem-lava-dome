#include <string.h>
#include "hand_eval.h"

/* Points per class, straight out of the web build's HAND_POINTS. The dome's
   threshold starts at 10 and climbs 5 a round, so round 1 is beaten by exactly
   One Pair and round 15 needs a Flush. Change a number here and you have
   changed the difficulty curve, not the wording. */
static const int POINTS[HAND_CLASS_COUNT] = {
    [HAND_HIGH_CARD]       = 0,
    [HAND_ONE_PAIR]        = 10,
    [HAND_TWO_PAIR]        = 25,
    [HAND_THREE_OF_A_KIND] = 50,
    [HAND_STRAIGHT]        = 75,
    [HAND_FLUSH]           = 100,
    [HAND_FULL_HOUSE]      = 150,
    [HAND_FOUR_OF_A_KIND]  = 250,
    [HAND_STRAIGHT_FLUSH]  = 500,
    [HAND_ROYAL_FLUSH]     = 1000
};

static const char *NAMES[HAND_CLASS_COUNT] = {
    "High Card", "One Pair", "Two Pair", "Three of a Kind", "Straight",
    "Flush", "Full House", "Four of a Kind", "Straight Flush", "Royal Flush"
};

int hand_points(HandRank rank) {
    if (rank < 0 || rank >= HAND_CLASS_COUNT) return 0;
    return POINTS[rank];
}

const char *hand_name(HandRank rank) {
    if (rank < 0 || rank >= HAND_CLASS_COUNT) return "No Hand";
    return NAMES[rank];
}

/* Descending, so tiebreakers read most-significant first everywhere below. */
static void sort_desc(int *v, int n) {
    for (int i = 1; i < n; i++) {
        int key = v[i], j = i - 1;
        while (j >= 0 && v[j] < key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
}

static int is_flush(const Card *c) {
    for (int i = 1; i < 5; i++) {
        if (c[i].suit != c[0].suit) return 0;
    }
    return 1;
}

/* `desc` is the five ranks, already sorted descending. Returns the rank the
   straight is named by, or 0 when it is not one.
 *
 * The wheel is the whole reason this returns a value rather than a flag. In
 * A-2-3-4-5 the ace plays low, so the hand is five-high, not ace-high -- get
 * that wrong and the wheel silently outranks every other straight. */
static int straight_high(const int *desc) {
    int run = 1;
    for (int i = 0; i < 4; i++) {
        if (desc[i] - desc[i + 1] != 1) { run = 0; break; }
    }
    if (run) return desc[0];

    if (desc[0] == RANK_ACE && desc[1] == 5 && desc[2] == 4 &&
        desc[3] == 3 && desc[4] == 2) {
        return 5;
    }
    return 0;
}

HandResult hand_eval_five(const Card *cards) {
    HandResult r;
    memset(&r, 0, sizeof(r));
    r.valid = 1;

    int desc[5];
    for (int i = 0; i < 5; i++) desc[i] = cards[i].rank;
    sort_desc(desc, 5);

    /* Ranks paired with how many of each, ordered by count then by rank, which
       is exactly the order the tiebreakers want: the trips before the pair, the
       quad before its kicker. */
    int rank_of[5], count_of[5], groups = 0;
    for (int i = 0; i < 5; i++) {
        int found = 0;
        for (int g = 0; g < groups; g++) {
            if (rank_of[g] == desc[i]) { count_of[g]++; found = 1; break; }
        }
        if (!found) {
            rank_of[groups] = desc[i];
            count_of[groups] = 1;
            groups++;
        }
    }
    for (int i = 1; i < groups; i++) {
        int kr = rank_of[i], kc = count_of[i], j = i - 1;
        while (j >= 0 && (count_of[j] < kc ||
                         (count_of[j] == kc && rank_of[j] < kr))) {
            rank_of[j + 1] = rank_of[j];
            count_of[j + 1] = count_of[j];
            j--;
        }
        rank_of[j + 1] = kr;
        count_of[j + 1] = kc;
    }

    int flush = is_flush(cards);
    int high  = straight_high(desc);
    int top   = count_of[0];
    int second = groups > 1 ? count_of[1] : 0;

    if (flush && high == RANK_ACE) {
        r.rank = HAND_ROYAL_FLUSH;
        r.tiebreak[0] = RANK_ACE;
        r.tiebreak_count = 1;
    } else if (flush && high) {
        r.rank = HAND_STRAIGHT_FLUSH;
        r.tiebreak[0] = high;
        r.tiebreak_count = 1;
    } else if (top == 4) {
        r.rank = HAND_FOUR_OF_A_KIND;
        r.tiebreak[0] = rank_of[0];
        r.tiebreak[1] = rank_of[1];
        r.tiebreak_count = 2;
    } else if (top == 3 && second == 2) {
        r.rank = HAND_FULL_HOUSE;
        r.tiebreak[0] = rank_of[0];
        r.tiebreak[1] = rank_of[1];
        r.tiebreak_count = 2;
    } else if (flush) {
        r.rank = HAND_FLUSH;
        for (int i = 0; i < 5; i++) r.tiebreak[i] = desc[i];
        r.tiebreak_count = 5;
    } else if (high) {
        r.rank = HAND_STRAIGHT;
        r.tiebreak[0] = high;
        r.tiebreak_count = 1;
    } else {
        /* Everything left is decided by the grouped order alone: trips, two
           pair, one pair and high card all read their groups most-significant
           first, which the sort above already arranged. */
        if      (top == 3) r.rank = HAND_THREE_OF_A_KIND;
        else if (top == 2 && second == 2) r.rank = HAND_TWO_PAIR;
        else if (top == 2) r.rank = HAND_ONE_PAIR;
        else               r.rank = HAND_HIGH_CARD;

        for (int i = 0; i < groups && i < HAND_TIEBREAK_MAX; i++) {
            r.tiebreak[i] = rank_of[i];
        }
        r.tiebreak_count = groups < HAND_TIEBREAK_MAX ? groups : HAND_TIEBREAK_MAX;
    }

    r.points = POINTS[r.rank];
    return r;
}

int hand_compare(const HandResult *a, const HandResult *b) {
    if (a->rank != b->rank) return a->rank < b->rank ? -1 : 1;

    int n = a->tiebreak_count < b->tiebreak_count
          ? a->tiebreak_count : b->tiebreak_count;
    for (int i = 0; i < n; i++) {
        if (a->tiebreak[i] != b->tiebreak[i]) {
            return a->tiebreak[i] < b->tiebreak[i] ? -1 : 1;
        }
    }
    return 0;
}

HandResult hand_eval_partial(const Card *cards, int count) {
    HandResult r;
    memset(&r, 0, sizeof(r));

    if (count < 2) return r;   /* invalid: nothing to say yet */
    r.valid = 1;

    int desc[HAND_MAX_CARDS];
    for (int i = 0; i < count; i++) desc[i] = cards[i].rank;
    sort_desc(desc, count);

    int best_count = 1, pairs = 0;
    for (int i = 0; i < count; i++) {
        int n = 0;
        for (int j = 0; j < count; j++) if (desc[j] == desc[i]) n++;
        if (n > best_count) best_count = n;
    }
    for (int i = 0; i < count; i++) {
        int n = 0;
        for (int j = 0; j < count; j++) if (desc[j] == desc[i]) n++;
        if (n == 2 && (i == 0 || desc[i] != desc[i - 1])) pairs++;
    }

    if      (best_count >= 4) r.rank = HAND_FOUR_OF_A_KIND;
    else if (best_count == 3 && pairs >= 1) r.rank = HAND_FULL_HOUSE;
    else if (best_count == 3) r.rank = HAND_THREE_OF_A_KIND;
    else if (pairs >= 2)      r.rank = HAND_TWO_PAIR;
    else if (pairs == 1)      r.rank = HAND_ONE_PAIR;
    else                      r.rank = HAND_HIGH_CARD;

    for (int i = 0; i < count && i < HAND_TIEBREAK_MAX; i++) r.tiebreak[i] = desc[i];
    r.tiebreak_count = count < HAND_TIEBREAK_MAX ? count : HAND_TIEBREAK_MAX;

    /* Zero, always. A hand that is not finished has not beaten the dome, and a
       partial result carrying real points is one careless call away from
       resolving a round early. */
    r.points = 0;
    return r;
}

HandResult hand_eval(const Card *cards, int count) {
    HandResult best;
    memset(&best, 0, sizeof(best));

    if (count < 5 || count > HAND_MAX_CARDS) return best;   /* invalid */

    /* Every five-card subset. At seven cards that is 21 of them, which is
       cheaper than any cleverness would be and impossible to get subtly wrong. */
    int idx[5];
    int have_best = 0;

    for (idx[0] = 0;        idx[0] < count - 4; idx[0]++)
    for (idx[1] = idx[0]+1; idx[1] < count - 3; idx[1]++)
    for (idx[2] = idx[1]+1; idx[2] < count - 2; idx[2]++)
    for (idx[3] = idx[2]+1; idx[3] < count - 1; idx[3]++)
    for (idx[4] = idx[3]+1; idx[4] < count;     idx[4]++) {
        Card combo[5];
        for (int i = 0; i < 5; i++) combo[i] = cards[idx[i]];

        HandResult r = hand_eval_five(combo);
        if (!have_best || hand_compare(&r, &best) > 0) {
            best = r;
            have_best = 1;
        }
    }

    return best;
}
