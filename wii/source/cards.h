#ifndef CARDS_H
#define CARDS_H

/* A standard 52-card deck.
 *
 * Aces are HIGH here -- an ace is 14, not 1 -- and that is the single most
 * important line in this header. The web build's shared evaluator carries
 * ace-LOW rank values (A === 1) and reads `value` straight off whatever card
 * object it is handed, so the browser version has to restamp every dealt card
 * ace-high in Dealer._draw() to make aces score correctly. Rather than port that
 * dance, the deck is ace-high from the start and nothing downstream has to
 * remember to fix it.
 *
 * The one place an ace still plays low is the wheel, A-2-3-4-5, which the
 * evaluator handles as a special case. See hand_eval.c.
 */

typedef enum {
    SUIT_CLUBS,
    SUIT_DIAMONDS,
    SUIT_HEARTS,
    SUIT_SPADES,
    SUIT_COUNT
} Suit;

#define RANK_MIN    2
#define RANK_JACK  11
#define RANK_QUEEN 12
#define RANK_KING  13
#define RANK_ACE   14
#define RANK_MAX   RANK_ACE

#define DECK_SIZE  52

typedef struct {
    int rank;   /* RANK_MIN..RANK_ACE */
    int suit;   /* Suit */
} Card;

/* Deterministic RNG, kept in the game rather than taken from rand().
 *
 * Two reasons. A shuffle is the one place in this game where the quality of the
 * randomness is the fairness of the game, and rand() on a given libc is whatever
 * it is. And a seeded generator means a hand that misbehaves can be replayed
 * exactly, in a test, on a laptop -- which is not true of anything seeded from
 * the clock. */
typedef struct {
    unsigned int state;
} Rng;

void         rng_seed(Rng *r, unsigned int seed);
unsigned int rng_next(Rng *r);
/* Uniform in 0..n-1, rejecting the biased tail rather than taking the modulo
   and hoping. Returns 0 when n < 1. */
int          rng_below(Rng *r, int n);

typedef struct {
    Card cards[DECK_SIZE];
    int  dealt;      /* how many have been drawn off the top */
} Deck;

/* Fills in rank order, suit by suit. Always follow with deck_shuffle() -- an
   unshuffled deck is a legal deck and a terrible game. */
void deck_init(Deck *d);
void deck_shuffle(Deck *d, Rng *r);

/* Draws the next card. Returns 1 on success. A deck cannot run out during a
   hand -- 2 hole plus 5 community is 7 of 52 -- so exhaustion means a bug
   somewhere else, and the caller is told rather than handed a silent duplicate. */
int  deck_draw(Deck *d, Card *out);
int  deck_remaining(const Deck *d);

/* "2".."10", "J", "Q", "K", "A". Valid until the next call for the same rank. */
const char *card_rank_label(int rank);

/* Single letters: C, D, H, S.
 *
 * The web build uses the suit glyphs, which Press Start 2P does not have. A
 * missing glyph on a CRT is indistinguishable from a bug, so the text form is
 * letters; drawing real pips is the renderer's job, not this header's. */
const char *card_suit_label(int suit);

/* Whether the suit is printed red. Rendering reads this rather than deciding
   for itself, so a card reads the same everywhere it appears. */
int card_suit_is_red(int suit);

#endif
