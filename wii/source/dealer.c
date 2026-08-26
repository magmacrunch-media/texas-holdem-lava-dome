#include <string.h>
#include "dealer.h"

void dealer_start_round(Dealer *dl, Rng *rng) {
    memset(dl, 0, sizeof(*dl));
    deck_init(&dl->deck);
    deck_shuffle(&dl->deck, rng);

    for (int i = 0; i < HOLE_CARDS; i++) {
        if (deck_draw(&dl->deck, &dl->hole[i])) dl->hole_count++;
    }
}

/* Deals `n` more community cards, but only from the count the caller expects to
   be at -- so a stray second call to dealer_flop() does nothing rather than
   quietly putting five cards on the board before the turn. */
static int deal_community(Dealer *dl, int expected_count, int n) {
    if (dl->community_count != expected_count) return dl->community_count;

    for (int i = 0; i < n && dl->community_count < COMMUNITY_CARDS; i++) {
        if (deck_draw(&dl->deck, &dl->community[dl->community_count])) {
            dl->community_count++;
        }
    }
    return dl->community_count;
}

int dealer_flop(Dealer *dl)  { return deal_community(dl, 0, FLOP_CARDS); }
int dealer_turn(Dealer *dl)  { return deal_community(dl, 3, 1); }
int dealer_river(Dealer *dl) { return deal_community(dl, 4, 1); }

int dealer_all_cards(const Dealer *dl, Card *out) {
    int n = 0;
    for (int i = 0; i < dl->hole_count; i++)      out[n++] = dl->hole[i];
    for (int i = 0; i < dl->community_count; i++) out[n++] = dl->community[i];
    return n;
}
