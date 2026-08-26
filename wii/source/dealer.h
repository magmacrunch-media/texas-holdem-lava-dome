#ifndef DEALER_H
#define DEALER_H

#include "cards.h"

/* Dealing a Hold'Em round: two hole cards, then the flop, turn and river off a
 * freshly shuffled deck.
 *
 * Small enough to be inlined into the loop and kept separate anyway, because
 * the loop should not be the thing that owns a deck. The one rule worth stating
 * is that a round deals from a full shuffle -- no burn cards, no carry-over
 * between rounds -- so nothing a player saw last round tells them anything
 * about this one.
 */

#define HOLE_CARDS       2
#define COMMUNITY_CARDS  5
#define FLOP_CARDS       3

typedef struct {
    Deck deck;
    Card hole[HOLE_CARDS];
    Card community[COMMUNITY_CARDS];
    int  hole_count;
    int  community_count;
} Dealer;

/* Fresh shuffle, hole cards dealt, board empty. */
void dealer_start_round(Dealer *dl, Rng *rng);

/* Each returns the number of community cards now face up. Calling one out of
   order, or twice, is ignored -- the board only ever grows to five. */
int dealer_flop(Dealer *dl);
int dealer_turn(Dealer *dl);
int dealer_river(Dealer *dl);

/* Hole plus community, in one array, for the evaluator. Returns the count. */
int dealer_all_cards(const Dealer *dl, Card *out);

#endif
