/* The hand evaluator, checked against arithmetic rather than against itself.
 *
 * The browser version of this game has no test suite, so there is nothing to
 * port across and no reference implementation to agree with. A hand-written list
 * of cases would only prove that the evaluator matches whatever the person
 * writing the cases believed at the time, which for poker hands is exactly the
 * thing in doubt -- the wheel, the ace-high straight, a flush that is also a
 * straight, quads that look like a full house if you count carelessly.
 *
 * So instead: deal every five-card hand that exists, classify each one, and
 * check the ten totals against the published counts. There are 2,598,960 such
 * hands and the distribution is not a matter of opinion. An evaluator that
 * produces all ten numbers is right in a way no case list can be, it fails
 * loudly and specifically when it is wrong, and the whole sweep runs in about a
 * second.
 *
 *   make test
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "cards.h"
#include "hand_eval.h"

/* The published frequencies of each five-card hand class. These are properties
   of a 52-card deck, not of this code. */
static const struct {
    HandRank rank;
    long     expected;
} FREQUENCIES[] = {
    { HAND_ROYAL_FLUSH,            4L },
    { HAND_STRAIGHT_FLUSH,        36L },
    { HAND_FOUR_OF_A_KIND,       624L },
    { HAND_FULL_HOUSE,          3744L },
    { HAND_FLUSH,               5108L },
    { HAND_STRAIGHT,           10200L },
    { HAND_THREE_OF_A_KIND,    54912L },
    { HAND_TWO_PAIR,          123552L },
    { HAND_ONE_PAIR,         1098240L },
    { HAND_HIGH_CARD,        1302540L }
};

#define TOTAL_FIVE_CARD_HANDS 2598960L

static void test_full_enumeration(void) {
    printf("hand_eval: every five-card hand in the deck\n");

    Deck deck;
    deck_init(&deck);

    long seen[HAND_CLASS_COUNT];
    memset(seen, 0, sizeof(seen));
    long total = 0;

    int a, b, c, d, e;
    for (a = 0;     a < DECK_SIZE - 4; a++)
    for (b = a + 1; b < DECK_SIZE - 3; b++)
    for (c = b + 1; c < DECK_SIZE - 2; c++)
    for (d = c + 1; d < DECK_SIZE - 1; d++)
    for (e = d + 1; e < DECK_SIZE;     e++) {
        Card hand[5] = {
            deck.cards[a], deck.cards[b], deck.cards[c],
            deck.cards[d], deck.cards[e]
        };
        HandResult r = hand_eval_five(hand);
        if (r.rank >= 0 && r.rank < HAND_CLASS_COUNT) seen[r.rank]++;
        total++;
    }

    printf("  classified %ld hands\n", total);

    /* The total is self-checking: if the ten classes sum correctly but the
       total is wrong, the loop is broken rather than the evaluator. */
    check(total == TOTAL_FIVE_CARD_HANDS, "the deck yields C(52,5) hands");

    for (unsigned i = 0; i < sizeof(FREQUENCIES) / sizeof(FREQUENCIES[0]); i++) {
        HandRank rank = FREQUENCIES[i].rank;
        long want = FREQUENCIES[i].expected;
        long got  = seen[rank];

        checks++;
        if (got != want) {
            printf("  FAIL: %-16s count (got %ld, want %ld)\n",
                   hand_name(rank), got, want);
            failures++;
        }
    }
}

/* Helpers for the named cases below. */
static Card cd(int rank, int suit) {
    Card c = { rank, suit };
    return c;
}

static void test_named_hands(void) {
    printf("hand_eval: the hands that are easy to get wrong\n");

    /* The wheel. An ace plays LOW here, so this is five-high -- if the ace is
       counted high the wheel outranks every other straight, which is the
       classic way to get this wrong. */
    Card wheel[5] = { cd(RANK_ACE, SUIT_CLUBS), cd(2, SUIT_HEARTS),
                      cd(3, SUIT_SPADES), cd(4, SUIT_DIAMONDS),
                      cd(5, SUIT_CLUBS) };
    HandResult w = hand_eval_five(wheel);
    check_int(w.rank, HAND_STRAIGHT, "A-2-3-4-5 is a straight");
    check_int(w.tiebreak[0], 5, "and it is five-high, not ace-high");

    /* The other end. */
    Card broadway[5] = { cd(RANK_ACE, SUIT_CLUBS), cd(RANK_KING, SUIT_HEARTS),
                         cd(RANK_QUEEN, SUIT_SPADES), cd(RANK_JACK, SUIT_DIAMONDS),
                         cd(10, SUIT_CLUBS) };
    HandResult bw = hand_eval_five(broadway);
    check_int(bw.rank, HAND_STRAIGHT, "10-J-Q-K-A is a straight");
    check_int(bw.tiebreak[0], RANK_ACE, "and it is ace-high");

    /* Same five ranks, one suit: royal, not merely a straight flush. */
    Card royal[5] = { cd(RANK_ACE, SUIT_SPADES), cd(RANK_KING, SUIT_SPADES),
                      cd(RANK_QUEEN, SUIT_SPADES), cd(RANK_JACK, SUIT_SPADES),
                      cd(10, SUIT_SPADES) };
    check_int(hand_eval_five(royal).rank, HAND_ROYAL_FLUSH, "suited broadway is royal");

    /* The wheel in one suit is a straight flush, NOT a royal -- it contains an
       ace, which is the trap. */
    Card steel[5] = { cd(RANK_ACE, SUIT_HEARTS), cd(2, SUIT_HEARTS),
                      cd(3, SUIT_HEARTS), cd(4, SUIT_HEARTS), cd(5, SUIT_HEARTS) };
    HandResult st = hand_eval_five(steel);
    check_int(st.rank, HAND_STRAIGHT_FLUSH, "the suited wheel is a straight flush");
    check_int(st.tiebreak[0], 5, "and still five-high");

    /* Nearly a straight, and not one. */
    Card gap[5] = { cd(9, SUIT_CLUBS), cd(10, SUIT_HEARTS), cd(RANK_JACK, SUIT_SPADES),
                    cd(RANK_QUEEN, SUIT_DIAMONDS), cd(RANK_ACE, SUIT_CLUBS) };
    check_int(hand_eval_five(gap).rank, HAND_HIGH_CARD, "9-10-J-Q-A is not a straight");

    /* Grouped hands, and the order their tiebreakers must come out in. */
    Card quads[5] = { cd(7, SUIT_CLUBS), cd(7, SUIT_HEARTS), cd(7, SUIT_SPADES),
                      cd(7, SUIT_DIAMONDS), cd(RANK_KING, SUIT_CLUBS) };
    HandResult q = hand_eval_five(quads);
    check_int(q.rank, HAND_FOUR_OF_A_KIND, "four sevens are quads");
    check_int(q.tiebreak[0], 7, "the quad rank leads the tiebreak");
    check_int(q.tiebreak[1], RANK_KING, "the kicker follows it");

    Card boat[5] = { cd(4, SUIT_CLUBS), cd(4, SUIT_HEARTS), cd(4, SUIT_SPADES),
                     cd(9, SUIT_DIAMONDS), cd(9, SUIT_CLUBS) };
    HandResult fh = hand_eval_five(boat);
    check_int(fh.rank, HAND_FULL_HOUSE, "trips over a pair is a full house");
    check_int(fh.tiebreak[0], 4, "the trips rank leads, even when lower");
    check_int(fh.tiebreak[1], 9, "the pair follows");

    Card twop[5] = { cd(3, SUIT_CLUBS), cd(3, SUIT_HEARTS), cd(RANK_JACK, SUIT_SPADES),
                     cd(RANK_JACK, SUIT_DIAMONDS), cd(6, SUIT_CLUBS) };
    HandResult tp = hand_eval_five(twop);
    check_int(tp.rank, HAND_TWO_PAIR, "two pair");
    check_int(tp.tiebreak[0], RANK_JACK, "the higher pair leads");
    check_int(tp.tiebreak[1], 3, "then the lower pair");
    check_int(tp.tiebreak[2], 6, "then the kicker");

    /* Points are what the dome is beaten with, so they are checked by name. */
    check_int(hand_points(HAND_HIGH_CARD), 0, "high card is worth nothing");
    check_int(hand_points(HAND_ONE_PAIR), 10, "one pair is exactly round 1's threshold");
    check_int(hand_points(HAND_FLUSH), 100, "a flush is worth 100");
    check_int(hand_points(HAND_ROYAL_FLUSH), 1000, "a royal is worth 1000");
}

static void test_comparisons(void) {
    printf("hand_eval: one hand against another\n");

    Card aces[5]  = { cd(RANK_ACE, SUIT_CLUBS), cd(RANK_ACE, SUIT_HEARTS),
                      cd(3, SUIT_SPADES), cd(7, SUIT_DIAMONDS), cd(9, SUIT_CLUBS) };
    Card kings[5] = { cd(RANK_KING, SUIT_CLUBS), cd(RANK_KING, SUIT_HEARTS),
                      cd(3, SUIT_SPADES), cd(7, SUIT_DIAMONDS), cd(9, SUIT_CLUBS) };
    HandResult a = hand_eval_five(aces), k = hand_eval_five(kings);
    check(hand_compare(&a, &k) > 0, "aces beat kings on the pair rank");
    check(hand_compare(&k, &a) < 0, "and the comparison is symmetric");
    check(hand_compare(&a, &a) == 0, "a hand ties itself");

    /* Same pair, different kicker: settled further down the tiebreak. */
    Card kick_hi[5] = { cd(5, SUIT_CLUBS), cd(5, SUIT_HEARTS), cd(RANK_ACE, SUIT_SPADES),
                        cd(7, SUIT_DIAMONDS), cd(2, SUIT_CLUBS) };
    Card kick_lo[5] = { cd(5, SUIT_SPADES), cd(5, SUIT_DIAMONDS), cd(RANK_KING, SUIT_CLUBS),
                        cd(7, SUIT_HEARTS), cd(2, SUIT_SPADES) };
    HandResult hi = hand_eval_five(kick_hi), lo = hand_eval_five(kick_lo);
    check(hand_compare(&hi, &lo) > 0, "the better kicker wins a shared pair");
}

/* Seven cards is 133 million combinations, too many for a test that runs on
 * every build. The property below is the part worth holding: the best hand out
 * of seven can never be worse than any five of those seven, because those five
 * were available to choose. It catches a best-of selection that compares wrongly
 * or skips combinations, which is what the seven-card path adds over five.
 */
static void test_best_of_seven(void) {
    printf("hand_eval: best of seven is never worse than any five of them\n");

    Rng rng;
    rng_seed(&rng, 20260820u);   /* seeded, so a failure replays exactly */

    int violations = 0, hands = 0;
    char first[160] = "";

    for (int trial = 0; trial < 20000; trial++) {
        Deck deck;
        deck_init(&deck);
        deck_shuffle(&deck, &rng);

        Card seven[7];
        for (int i = 0; i < 7; i++) deck_draw(&deck, &seven[i]);

        HandResult best = hand_eval(seven, 7);
        hands++;

        int a, b, c, d, e;
        for (a = 0;     a < 3; a++)
        for (b = a + 1; b < 4; b++)
        for (c = b + 1; c < 5; c++)
        for (d = c + 1; d < 6; d++)
        for (e = d + 1; e < 7; e++) {
            Card five[5] = { seven[a], seven[b], seven[c], seven[d], seven[e] };
            HandResult sub = hand_eval_five(five);
            if (hand_compare(&best, &sub) < 0 && !violations++) {
                snprintf(first, sizeof(first),
                         "trial %d: best-of-seven was %s, but the subset "
                         "%d,%d,%d,%d,%d makes %s",
                         trial, hand_name(best.rank), a, b, c, d, e,
                         hand_name(sub.rank));
            }
        }
    }

    printf("  checked %d seven-card hands against all 21 of their subsets\n", hands);
    if (violations) printf("  first violation: %s\n", first);
    check_int(violations, 0, "no five-card subset ever beats the chosen best");
}

static void test_partial(void) {
    printf("hand_eval: part-dealt hands, for display only\n");

    Card pair[2] = { cd(RANK_QUEEN, SUIT_CLUBS), cd(RANK_QUEEN, SUIT_HEARTS) };
    HandResult p = hand_eval_partial(pair, 2);
    check_int(p.rank, HAND_ONE_PAIR, "two of a rank in the hole is a pair");
    check_int(p.points, 0, "but a partial hand is worth no points");
    check(p.valid, "and it is still a usable result");

    Card off[2] = { cd(RANK_ACE, SUIT_CLUBS), cd(7, SUIT_HEARTS) };
    check_int(hand_eval_partial(off, 2).rank, HAND_HIGH_CARD, "offsuit junk is high card");

    Card trips[4] = { cd(8, SUIT_CLUBS), cd(8, SUIT_HEARTS), cd(8, SUIT_SPADES),
                      cd(2, SUIT_DIAMONDS) };
    check_int(hand_eval_partial(trips, 4).rank, HAND_THREE_OF_A_KIND, "trips before the turn");

    /* Fewer than five cards must never come back as a scoreable hand. */
    HandResult too_few = hand_eval(pair, 2);
    check(!too_few.valid, "hand_eval refuses to score two cards");
    check_int(too_few.points, 0, "and reports no points");
}

int main(void) {
    test_full_enumeration();
    test_named_hands();
    test_comparisons();
    test_best_of_seven();
    test_partial();
    return report();
}
