/* The dealer: a fresh shuffle, two hole cards, then the board grown one street
 * at a time.
 *
 * Small enough to look self-evidently correct, which is exactly the file a
 * test suite skips and exactly the file that then goes unexercised except as a
 * side effect of testing hand_eval or dome. A soak run has already found real
 * bugs living in code nothing else was checking directly -- this closes that
 * gap for the one host-clean module that had no suite of its own.
 *
 * The guard worth pinning down is deal_community()'s expected_count check: it
 * is the one piece of actual logic in dealer.c, and the part most likely to
 * regress silently, because "the board grew" and "the board grew correctly"
 * look identical from three of the four call sites.
 *
 *   make test
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "cards.h"
#include "dealer.h"

static void test_start_round(void) {
    printf("dealer: starting a round\n");

    Rng rng;
    rng_seed(&rng, 1);

    Dealer dl;
    dealer_start_round(&dl, &rng);

    check_int(dl.hole_count, HOLE_CARDS, "two hole cards are dealt");
    check_int(dl.community_count, 0, "and the board is empty");

    for (int i = 0; i < dl.hole_count; i++) {
        check(dl.hole[i].rank >= RANK_MIN && dl.hole[i].rank <= RANK_ACE,
              "a hole card's rank is a real rank");
        check(dl.hole[i].suit >= 0 && dl.hole[i].suit < SUIT_COUNT,
              "and its suit is a real suit");
    }

    check(dl.hole[0].rank != dl.hole[1].rank || dl.hole[0].suit != dl.hole[1].suit,
          "the two hole cards are not the same card");
}

/* The guard: each street only advances from the exact count it expects, in
   both directions -- out of order does nothing, and twice in a row does
   nothing the second time. */
static void test_streets_advance_in_order(void) {
    printf("dealer: the board grows one street at a time\n");

    Rng rng;
    rng_seed(&rng, 2);

    Dealer dl;
    dealer_start_round(&dl, &rng);

    check_int(dealer_turn(&dl), 0, "the turn does nothing before the flop");
    check_int(dl.community_count, 0, "and the board is still empty");
    check_int(dealer_river(&dl), 0, "neither does the river");
    check_int(dl.community_count, 0, "still empty");

    check_int(dealer_flop(&dl), FLOP_CARDS, "the flop deals three");
    check_int(dl.community_count, 3, "and the board shows three");

    check_int(dealer_flop(&dl), 3, "a second flop call does nothing");
    check_int(dl.community_count, 3, "the board is unchanged");

    check_int(dealer_river(&dl), 3, "the river still refuses before the turn");
    check_int(dl.community_count, 3, "unchanged");

    check_int(dealer_turn(&dl), 4, "the turn deals one, once the flop is down");
    check_int(dl.community_count, 4, "four on the board");

    check_int(dealer_turn(&dl), 4, "a second turn call does nothing");

    check_int(dealer_river(&dl), 5, "the river deals the last one");
    check_int(dl.community_count, 5, "five on the board, the full house of it");

    check_int(dealer_river(&dl), 5, "a second river call does nothing");
    check_int(dealer_flop(&dl), 5, "and the board is done growing entirely -- "
              "a stray flop call after the river is also a no-op");
    check_int(dl.community_count, 5, "still five");
}

/* The property a count-only check cannot see: every card the round hands out
   is distinct. A shuffle or draw bug that hands out the same card twice would
   pass every count above and still be wrong. */
static void test_full_round_has_no_duplicates(void) {
    printf("dealer: a full round deals seven distinct cards\n");

    Rng rng;
    rng_seed(&rng, 3);

    Dealer dl;
    dealer_start_round(&dl, &rng);
    dealer_flop(&dl);
    dealer_turn(&dl);
    dealer_river(&dl);

    Card all[HOLE_CARDS + COMMUNITY_CARDS];
    int n = dealer_all_cards(&dl, all);
    check_int(n, 7, "hole plus a full board is seven cards");

    int dup = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (all[i].rank == all[j].rank && all[i].suit == all[j].suit) dup++;
        }
    }
    check_int(dup, 0, "no card appears twice");
}

static void test_all_cards_matches_the_arrays(void) {
    printf("dealer: dealer_all_cards agrees with hole and community\n");

    Rng rng;
    rng_seed(&rng, 4);

    Dealer dl;
    dealer_start_round(&dl, &rng);
    dealer_flop(&dl);

    Card all[HOLE_CARDS + COMMUNITY_CARDS];
    int n = dealer_all_cards(&dl, all);

    check_int(n, dl.hole_count + dl.community_count, "the count is hole plus community");

    for (int i = 0; i < dl.hole_count; i++) {
        check(all[i].rank == dl.hole[i].rank && all[i].suit == dl.hole[i].suit,
              "hole cards come first, in order");
    }
    for (int i = 0; i < dl.community_count; i++) {
        check(all[dl.hole_count + i].rank == dl.community[i].rank &&
              all[dl.hole_count + i].suit == dl.community[i].suit,
              "then community cards, in order");
    }
}

/* dealer_start_round() memsets the whole struct, so a second round must not
   read as a continuation of the first -- the board resets to empty and the
   hole cards are freshly dealt, not whatever the first round left in memory. */
static void test_second_round_starts_clean(void) {
    printf("dealer: a new round carries nothing over from the last one\n");

    Rng rng;
    rng_seed(&rng, 5);

    Dealer dl;
    dealer_start_round(&dl, &rng);
    dealer_flop(&dl);
    dealer_turn(&dl);
    dealer_river(&dl);
    check_int(dl.community_count, 5, "the first round ran to a full board");

    dealer_start_round(&dl, &rng);
    check_int(dl.community_count, 0, "the second round's board starts empty");
    check_int(dl.hole_count, HOLE_CARDS, "with two fresh hole cards");
}

/* The same property main.c already leans on for a reproducible session,
   checked at the layer that actually deals: two Rngs seeded alike deal alike. */
static void test_same_seed_deals_the_same_hand(void) {
    printf("dealer: a seed determines the deal\n");

    Rng rng_a, rng_b;
    rng_seed(&rng_a, 42);
    rng_seed(&rng_b, 42);

    Dealer a, b;
    dealer_start_round(&a, &rng_a);
    dealer_start_round(&b, &rng_b);
    dealer_flop(&a); dealer_flop(&b);
    dealer_turn(&a);  dealer_turn(&b);
    dealer_river(&a); dealer_river(&b);

    Card all_a[HOLE_CARDS + COMMUNITY_CARDS];
    Card all_b[HOLE_CARDS + COMMUNITY_CARDS];
    int n = dealer_all_cards(&a, all_a);
    dealer_all_cards(&b, all_b);

    check_int(memcmp(all_a, all_b, sizeof(Card) * n), 0,
              "identical seeds deal an identical seven cards");

    /* And a different seed is not obligated to agree -- this is not proving
       the shuffle is good (cards.c would be the place for that), only that the
       dealer is not secretly reaching for something unseeded. */
    Rng rng_c;
    rng_seed(&rng_c, 43);
    Dealer c;
    dealer_start_round(&c, &rng_c);
    check(c.hole[0].rank != a.hole[0].rank || c.hole[0].suit != a.hole[0].suit ||
          c.hole[1].rank != a.hole[1].rank || c.hole[1].suit != a.hole[1].suit,
          "a different seed is free to deal a different hand");
}

int main(void) {
    test_start_round();
    test_streets_advance_in_order();
    test_full_round_has_no_duplicates();
    test_all_cards_matches_the_arrays();
    test_second_round_starts_clean();
    test_same_seed_deals_the_same_hand();
    return report();
}
