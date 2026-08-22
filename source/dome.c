#include <string.h>
#include "dome.h"

/* The ante for rounds 1..15, straight from js/config.js. Past the end the last
   value climbs by DOME_ANTE_ESCALATION every round, so there is no round at
   which the pressure stops rising. Even perfect play runs out eventually --
   that is the point, and it is why knowing when to leave is the real decision. */
static const int ANTE_SCHEDULE[DOME_ANTE_ROUNDS] = {
    10, 10, 20, 20, 30, 30, 50, 50, 75, 75, 100, 125, 150, 200, 250
};

/* Depth labels, themed after the band's songs. Index is round - 1; past the end
   the deepest label sticks, because there is no song after the last one. */
static const char *DEPTH_LABELS[DOME_ANTE_ROUNDS] = {
    "I keep my cards close to my heart",
    "Eager for second chances",
    "Contemplate the Plate Tectonic",
    "Contemplate the Plate Tectonic",
    "Figure the Shoreline",
    "Figure the Shoreline",
    "Penultimate Drop",
    "Penultimate Drop",
    "Pendant Stop",
    "Pendant Stop",
    "Hazardous Metals in Ambient Air",
    "Hazardous Metals in Ambient Air",
    "I Would Go Up to the Hot Lava",
    "Millstone, 2063",
    "All All & All"
};

/* Payout multiplier per hand class, in hundredths. A Flush pays 2.5x and Two
   Pair 1.25x, so integers will not do and floats on this console are not worth
   it for money. High Card is 0: it cannot beat the dome at any threshold, so it
   never reaches a payout at all, and the zero says so out loud. */
static const int PAYOUT_HUNDREDTHS[HAND_CLASS_COUNT] = {
    [HAND_HIGH_CARD]       = 0,
    [HAND_ONE_PAIR]        = 100,
    [HAND_TWO_PAIR]        = 125,
    [HAND_THREE_OF_A_KIND] = 150,
    [HAND_STRAIGHT]        = 200,
    [HAND_FLUSH]           = 250,
    [HAND_FULL_HOUSE]      = 300,
    [HAND_FOUR_OF_A_KIND]  = 400,
    [HAND_STRAIGHT_FLUSH]  = 600,
    [HAND_ROYAL_FLUSH]     = 1000
};

/* The quips, verbatim from js/config.js. Kept as three tables of the same
   shape so picking one is the same code three times over rather than three
   slightly different ones. */
#define FLAVOR_COUNT 5

static const char *FLAVOR_WIN[FLAVOR_COUNT] = {
    "I keep my cards close to my heart.",
    "Maybe the instruments failed and maybe they didn't.",
    "This has always been true.",
    "Ocean of storms -- you surfed it.",
    "Hello World, Love Space."
};

/* Used for a lost hand as well as a real bust, which is what the web build
   does: dome.js reaches for this same array in resolveHand() and in _bust(). */
static const char *FLAVOR_BUST[FLAVOR_COUNT] = {
    "The dome has claimed another soul.",
    "Bus full of time-traveling twenty-somethings -- and you.",
    "What happened to you in all the confusion?",
    "The dome is not forgiving.",
    "Millstone, 2063. That's you now."
};

static const char *FLAVOR_ESCAPE[FLAVOR_COUNT] = {
    "You escaped the dome. For now.",
    "Friendship 7 4 fun -- and profit.",
    "A pine tree caught electrical fire, but not you.",
    "Secret conference rooms await.",
    "Rendezvous at 44i boo. Mission complete."
};

/* A null generator picks the first line rather than refusing to have anything
   to say. Flavour text is the one thing here that should never be the reason a
   screen comes up empty. */
static const char *pick_flavor(const char *const *table, Rng *rng) {
    return rng ? table[rng_below(rng, FLAVOR_COUNT)] : table[0];
}

const char *dome_flavor_win(Rng *rng)    { return pick_flavor(FLAVOR_WIN, rng); }
const char *dome_flavor_bust(Rng *rng)   { return pick_flavor(FLAVOR_BUST, rng); }
const char *dome_flavor_escape(Rng *rng) { return pick_flavor(FLAVOR_ESCAPE, rng); }

void dome_init(Dome *d) {
    memset(d, 0, sizeof(*d));
    d->chips = STARTING_CHIPS;
    d->phase = DOME_PHASE_IDLE;
}

int dome_ante_for_round(int round) {
    if (round < 1) round = 1;

    if (round <= DOME_ANTE_ROUNDS) return ANTE_SCHEDULE[round - 1];

    int beyond = round - DOME_ANTE_ROUNDS;
    return ANTE_SCHEDULE[DOME_ANTE_ROUNDS - 1] + beyond * DOME_ANTE_ESCALATION;
}

int dome_next_ante(const Dome *d) {
    return dome_ante_for_round(d->round + 1);
}

int dome_ante_danger(const Dome *d) {
    /* Deliberately <=, not <. A stack that exactly covers the next ante pays it
       and arrives at the betting screen with nothing, which is the same dead
       end as being short -- and the warning is worth more before it happens. */
    return !d->session_over && d->chips <= dome_next_ante(d);
}

int dome_threshold_for_round(int round) {
    if (round < 1) round = 1;
    return DOME_BASE_THRESHOLD + (round - 1) * DOME_THRESHOLD_SCALE;
}

const char *dome_depth_label(int round) {
    if (round < 1) round = 1;
    if (round > DOME_ANTE_ROUNDS) round = DOME_ANTE_ROUNDS;
    return DEPTH_LABELS[round - 1];
}

int dome_payout_hundredths(HandRank rank) {
    if (rank < 0 || rank >= HAND_CLASS_COUNT) return 0;
    return PAYOUT_HUNDREDTHS[rank];
}

int dome_score(const Dome *d) {
    return d->bank;
}

int dome_start_round(Dome *d) {
    /* Nothing in the stack and nothing in the bank is the end of it. A player
       with an empty stack but a filled bank is not bust here -- they have a
       score, and the ante is what will take the last of it. */
    if (d->chips <= 0 && d->bank <= 0) {
        d->session_over = 1;
        d->escaped = 0;
        return 0;
    }

    d->round++;
    d->phase = DOME_PHASE_IDLE;
    d->current_bet = 0;
    d->ante = 0;
    return 1;
}

int dome_charge_ante(Dome *d) {
    if (d->chips <= 0) {
        d->session_over = 1;
        d->escaped = 0;
        return 0;
    }

    int ante = dome_ante_for_round(d->round);

    /* Short is allowed: the player pays what they have and the dome takes it.
       The alternative -- refusing the round -- would leave a stack too small to
       ante sitting there forever. */
    int actual = ante < d->chips ? ante : d->chips;
    d->ante = actual;
    d->chips -= actual;

    if (d->chips == 0) {
        d->session_over = 1;
        d->escaped = 0;
        return 0;
    }

    d->phase = DOME_PHASE_BETTING;
    return 1;
}

int dome_min_bet(const Dome *d) {
    /* A stack smaller than the minimum can still be bet -- all of it. */
    return MIN_BET < d->chips ? MIN_BET : d->chips;
}

int dome_max_bet(const Dome *d) {
    return d->chips;
}

int dome_can_raise(const Dome *d) {
    return d->chips > 0 && !d->session_over;
}

RiskLevel dome_risk_level(const Dome *d, int amount) {
    if (d->chips <= 0) return RISK_ALL_IN;
    if (amount >= d->chips) return RISK_ALL_IN;

    /* Whole percent of the stack. The web build's thresholds are 0.40 and 0.75
       of a float ratio; 40 and 75 of an integer percent are the same lines. */
    int pct = amount * 100 / d->chips;
    if (pct >= 75) return RISK_HIGH;
    if (pct >= 40) return RISK_MEDIUM;
    return RISK_LOW;
}

const char *dome_risk_label(RiskLevel r) {
    switch (r) {
        case RISK_ALL_IN: return "ALL IN";
        case RISK_HIGH:   return "HIGH RISK";
        case RISK_MEDIUM: return "MEDIUM RISK";
        default:          return "LOW RISK";
    }
}

int dome_place_bet(Dome *d, int amount) {
    int lo = dome_min_bet(d);
    int hi = dome_max_bet(d);

    if (hi <= 0) return 0;
    if (amount < lo) amount = lo;
    if (amount > hi) amount = hi;

    d->chips -= amount;
    d->current_bet += amount;
    return amount;
}

DomeResult dome_resolve(Dome *d, const HandResult *hand, Rng *rng) {
    DomeResult res;
    memset(&res, 0, sizeof(res));

    res.hand      = hand->rank;
    res.points    = hand->points;
    res.threshold = dome_threshold_for_round(d->round);
    res.beat_dome = hand->points >= res.threshold;

    if (res.beat_dome) {
        /* Integer division truncates, which is the floor the web build takes
           explicitly -- the house keeps the fraction of a chip. */
        res.chips_won = (d->current_bet * dome_payout_hundredths(hand->rank)) / 100;
        d->chips += d->current_bet + res.chips_won;
    } else {
        res.chips_lost = d->current_bet;
        /* The stake was already taken when it was placed; losing is simply not
           getting it back. */
    }

    d->phase = DOME_PHASE_CASHOUT;
    d->current_bet = 0;

    res.flavor = res.beat_dome ? dome_flavor_win(rng) : dome_flavor_bust(rng);

    res.bust = (d->chips <= 0);
    if (res.bust && d->bank <= 0) {
        d->session_over = 1;
        d->escaped = 0;
    }

    return res;
}

DomeResult dome_fold(Dome *d, Rng *rng) {
    DomeResult res;
    memset(&res, 0, sizeof(res));

    /* No hand is read. A fold is not a loss on the merits and the result panel
       should not claim the dome out-played anything -- it says what happened
       and moves on. `hand` therefore stays HAND_HIGH_CARD with zero points,
       which is what an unexamined hand is worth. */
    res.folded     = 1;
    res.threshold  = dome_threshold_for_round(d->round);
    res.beat_dome  = 0;
    res.chips_lost = d->current_bet;
    res.flavor     = dome_flavor_bust(rng);

    d->phase = DOME_PHASE_CASHOUT;
    d->current_bet = 0;

    /* Folding an all-in empties the stack just as surely as losing it, so the
       same end-of-session test has to run here. Without it a player could fold
       their last chip and be handed a betting screen with nothing to bet. */
    res.bust = (d->chips <= 0);
    if (res.bust && d->bank <= 0) {
        d->session_over = 1;
        d->escaped = 0;
    }

    return res;
}

int dome_cash_out(Dome *d, int amount) {
    if (amount < 0) return 0;

    int actual = amount < d->chips ? amount : d->chips;
    d->chips -= actual;
    d->bank  += actual;
    return actual;
}

int dome_withdraw_from_bank(Dome *d, int amount) {
    if (amount < 0) return 0;

    int actual = amount < d->bank ? amount : d->bank;
    d->bank  -= actual;
    d->chips += actual;
    return actual;
}

int dome_must_dig_into_savings(const Dome *d) {
    /* MIN_BET rather than dome_min_bet(), deliberately. dome_min_bet() reports
       what is legal to bet given the stack -- with 3 chips it says 3, which is
       true and unhelpful here. The question this answers is whether the stack
       has fallen below a real bet while there is still money to fetch. */
    return d->chips < MIN_BET && d->bank > 0 && !d->session_over;
}

int dome_needs_savings_for_ante(const Dome *d) {
    if (d->session_over) return 0;
    return d->chips < dome_ante_for_round(d->round) && d->bank > 0;
}

int dome_can_escape(const Dome *d) {
    /* Both of the phases that mean "between rounds". CASHOUT is the result
       panel and IDLE is the decision that follows it, and a player who banks
       half their chips passes from one to the other without the question
       changing -- so checking only CASHOUT made the ESCAPE button vanish the
       moment anyone used the screen it lives on. What is being excluded is a
       hand in progress, and the two betting phases are still excluded. */
    if (d->phase != DOME_PHASE_CASHOUT && d->phase != DOME_PHASE_IDLE) return 0;
    return d->chips > 0 && !d->session_over;
}

void dome_escape(Dome *d) {
    dome_cash_out(d, d->chips);
    d->escaped = 1;
    d->session_over = 1;
}
