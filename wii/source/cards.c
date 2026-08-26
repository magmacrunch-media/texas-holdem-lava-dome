#include "cards.h"

/* xorshift32. Small, fast, no allocation, and the same sequence everywhere --
   which is the property the tests need and the console does not care about. */
void rng_seed(Rng *r, unsigned int seed) {
    /* A zero state is a fixed point for xorshift: it would return 0 forever, so
       a caller seeding from a clock that happens to read 0 would get a deck
       that never shuffles. */
    r->state = seed ? seed : 0x9E3779B9u;
}

unsigned int rng_next(Rng *r) {
    unsigned int x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

int rng_below(Rng *r, int n) {
    if (n < 1) return 0;

    /* Taking the modulo directly would favour the low values whenever n does
       not divide 2^32 evenly. At n = 52 the bias is tiny, but it is a bias in
       which cards come up, and that is not a thing to leave in a card game.
       Reject the short final block instead. */
    unsigned int limit = 0xFFFFFFFFu - (0xFFFFFFFFu % (unsigned int)n);
    unsigned int v;
    do {
        v = rng_next(r);
    } while (v >= limit);

    return (int)(v % (unsigned int)n);
}

void deck_init(Deck *d) {
    int i = 0;
    for (int s = 0; s < SUIT_COUNT; s++) {
        for (int rank = RANK_MIN; rank <= RANK_ACE; rank++) {
            d->cards[i].rank = rank;
            d->cards[i].suit = s;
            i++;
        }
    }
    d->dealt = 0;
}

void deck_shuffle(Deck *d, Rng *r) {
    /* Fisher-Yates, walking down. The off-by-one version of this -- picking
       from the whole deck each time instead of from the unshuffled remainder --
       produces a distribution that looks shuffled and is not. */
    for (int i = DECK_SIZE - 1; i > 0; i--) {
        int j = rng_below(r, i + 1);
        Card tmp = d->cards[i];
        d->cards[i] = d->cards[j];
        d->cards[j] = tmp;
    }
    d->dealt = 0;
}

int deck_draw(Deck *d, Card *out) {
    if (!out || d->dealt >= DECK_SIZE) return 0;
    *out = d->cards[d->dealt++];
    return 1;
}

int deck_remaining(const Deck *d) {
    return DECK_SIZE - d->dealt;
}

const char *card_rank_label(int rank) {
    static const char *labels[] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
    };
    if (rank < RANK_MIN || rank > RANK_ACE) return "?";
    return labels[rank - RANK_MIN];
}

const char *card_suit_label(int suit) {
    switch (suit) {
        case SUIT_CLUBS:    return "C";
        case SUIT_DIAMONDS: return "D";
        case SUIT_HEARTS:   return "H";
        case SUIT_SPADES:   return "S";
        default:            return "?";
    }
}

int card_suit_is_red(int suit) {
    return suit == SUIT_DIAMONDS || suit == SUIT_HEARTS;
}
