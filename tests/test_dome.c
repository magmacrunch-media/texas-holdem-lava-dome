/* The money: the ante schedule, the threshold curve, payouts, and the two ways
 * a session ends.
 *
 * These are the numbers the game is balanced on, and they are the easiest thing
 * in the port to get quietly wrong -- an off-by-one in the ante lookup or the
 * threshold curve gives a game that plays, and plays at the wrong difficulty,
 * with nothing on screen to say so.
 *
 *   make test-dome
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dome.h"
#include "hand_eval.h"

static void test_ante_schedule(void) {
    printf("dome: the ante schedule\n");

    /* The published schedule, round by round. Written out rather than looped so
       a transcription error shows as itself and not as a formula. */
    static const int expected[] = {
        10, 10, 20, 20, 30, 30, 50, 50, 75, 75, 100, 125, 150, 200, 250
    };
    for (int r = 1; r <= 15; r++) {
        char what[48];
        snprintf(what, sizeof(what), "round %d antes %d", r, expected[r - 1]);
        check_int(dome_ante_for_round(r), expected[r - 1], what);
    }

    /* Past the table the last value climbs forever, 25 a round. */
    check_int(dome_ante_for_round(16), 275, "round 16 escalates past the schedule");
    check_int(dome_ante_for_round(17), 300, "and keeps climbing");
    check_int(dome_ante_for_round(25), 500, "round 25 is 250 + 10 * 25");

    /* The boundary is where an off-by-one would live: round 15 must be the last
       scheduled value, not the first escalated one. */
    check_int(dome_ante_for_round(15), 250, "round 15 is still the schedule");
    check(dome_ante_for_round(16) > dome_ante_for_round(15),
          "the escalation starts after the schedule, not on top of its last entry");

    check_int(dome_ante_for_round(0), 10, "round 0 is treated as round 1");
    check_int(dome_ante_for_round(-5), 10, "and so is a negative round");
}

static void test_threshold_curve(void) {
    printf("dome: the dome's threshold\n");

    check_int(dome_threshold_for_round(1), 10, "round 1 needs 10 points");
    check_int(dome_threshold_for_round(2), 15, "and climbs by 5");
    check_int(dome_threshold_for_round(10), 55, "round 10 needs 55");

    /* The curve is only meaningful against the hand points, so check the two
       against each other rather than in isolation. */
    check_int(hand_points(HAND_ONE_PAIR), dome_threshold_for_round(1),
              "round 1 is beaten by exactly One Pair");
    check(hand_points(HAND_TWO_PAIR) >= dome_threshold_for_round(4),
          "two pair still clears round 4");
    check(hand_points(HAND_ONE_PAIR) < dome_threshold_for_round(2),
          "but one pair is already short by round 2");
    check(hand_points(HAND_FLUSH) >= dome_threshold_for_round(16),
          "a flush still clears round 16");

    /* High card can never win, at any depth. */
    check_int(hand_points(HAND_HIGH_CARD), 0, "high card is worth nothing");
    check(hand_points(HAND_HIGH_CARD) < dome_threshold_for_round(1),
          "so it cannot beat even round 1");
}

static void test_payouts(void) {
    printf("dome: payout multipliers\n");

    check_int(dome_payout_hundredths(HAND_HIGH_CARD), 0, "high card pays nothing");
    check_int(dome_payout_hundredths(HAND_ONE_PAIR), 100, "one pair pays even money");
    check_int(dome_payout_hundredths(HAND_TWO_PAIR), 125, "two pair pays 1.25x");
    check_int(dome_payout_hundredths(HAND_FLUSH), 250, "a flush pays 2.5x");
    check_int(dome_payout_hundredths(HAND_ROYAL_FLUSH), 1000, "a royal pays 10x");

    /* The fractional ones are why this is in hundredths at all: a 2.5x payout on
       an odd stake has to round somewhere, and the house keeps the fraction. */
    Dome d;
    dome_init(&d);
    dome_start_round(&d);
    dome_charge_ante(&d);
    dome_place_bet(&d, 101);

    HandResult flush;
    memset(&flush, 0, sizeof(flush));
    flush.rank = HAND_FLUSH;
    flush.points = hand_points(HAND_FLUSH);
    flush.valid = 1;

    DomeResult r = dome_resolve(&d, &flush);
    check_int(r.chips_won, 252, "2.5x on 101 chips floors to 252, not 253");
}

static void test_round_and_ante_charging(void) {
    printf("dome: starting a round and paying the dome\n");

    Dome d;
    dome_init(&d);
    check_int(d.chips, STARTING_CHIPS, "a session opens with the starting stack");
    check_int(d.bank, 0, "and an empty bank");
    check_int(d.round, 0, "before the first round");

    check(dome_start_round(&d), "the first round starts");
    check_int(d.round, 1, "and it is round 1");

    check(dome_charge_ante(&d), "the ante is paid");
    check_int(d.ante, 10, "round 1 costs 10");
    check_int(d.chips, 490, "which comes off the stack");

    /* A stack too small to cover the ante pays what it has and that is the end
       of the session -- the dome does not extend credit. */
    Dome poor;
    dome_init(&poor);
    poor.chips = 5;
    poor.round = 3;                       /* a 20 ante */
    check_int(dome_charge_ante(&poor), 0, "a short ante ends the session");
    check_int(poor.ante, 5, "the dome takes what there was");
    check_int(poor.chips, 0, "leaving nothing");
    check(poor.session_over, "and the session is over");
    check(!poor.escaped, "as a bust, not an escape");

    /* Exactly enough is still the end: paying the last chip empties the stack. */
    Dome exact;
    dome_init(&exact);
    exact.chips = 10;
    exact.round = 1;
    check_int(dome_charge_ante(&exact), 0, "an ante that takes the last chip ends it");
    check_int(exact.chips, 0, "with an empty stack");
}

static void test_betting(void) {
    printf("dome: bet sizing\n");

    Dome d;
    dome_init(&d);
    dome_start_round(&d);
    dome_charge_ante(&d);          /* 490 left */

    check_int(dome_min_bet(&d), MIN_BET, "the minimum is the configured floor");
    check_int(dome_max_bet(&d), 490, "the maximum is the whole stack -- all-in is legal");

    check_int(dome_place_bet(&d, 100), 100, "a legal bet is taken in full");
    check_int(d.chips, 390, "and deducted immediately");
    check_int(d.current_bet, 100, "and recorded as committed");

    /* Out-of-range bets clamp rather than fail: the UI should not be able to
       produce one, and if it does, the game continues at a legal amount. */
    check_int(dome_place_bet(&d, 99999), 390, "an oversized bet clamps to the stack");
    check_int(d.chips, 0, "which is all-in");
    check_int(d.current_bet, 490, "and adds to what was already committed");

    Dome small;
    dome_init(&small);
    small.chips = 4;
    check_int(dome_min_bet(&small), 4,
              "a stack below the minimum can still bet all of it");
}

static void test_resolution(void) {
    printf("dome: settling a hand against the dome\n");

    HandResult pair, junk;
    memset(&pair, 0, sizeof(pair));
    pair.rank = HAND_ONE_PAIR;
    pair.points = hand_points(HAND_ONE_PAIR);
    pair.valid = 1;

    memset(&junk, 0, sizeof(junk));
    junk.rank = HAND_HIGH_CARD;
    junk.points = 0;
    junk.valid = 1;

    /* A win returns the stake and pays on top of it. */
    Dome win;
    dome_init(&win);
    dome_start_round(&win);
    dome_charge_ante(&win);              /* 490 */
    dome_place_bet(&win, 100);           /* 390 */
    DomeResult w = dome_resolve(&win, &pair);
    check(w.beat_dome, "one pair beats round 1");
    check_int(w.chips_won, 100, "and pays even money");
    check_int(win.chips, 590, "so the stake comes back with the winnings");
    check_int(win.current_bet, 0, "and the round's bet is cleared");

    /* A loss simply does not return it. */
    Dome lose;
    dome_init(&lose);
    dome_start_round(&lose);
    dome_charge_ante(&lose);             /* 490 */
    dome_place_bet(&lose, 100);          /* 390 */
    DomeResult l = dome_resolve(&lose, &junk);
    check(!l.beat_dome, "high card cannot beat the dome");
    check_int(l.chips_lost, 100, "the stake is forfeit");
    check_int(lose.chips, 390, "and does not come back");

    /* The same hand loses later on, which is the whole escalation. */
    Dome deep;
    dome_init(&deep);
    deep.round = 5;                      /* threshold 30, above One Pair's 10 */
    deep.chips = 500;
    dome_place_bet(&deep, 50);
    DomeResult dr = dome_resolve(&deep, &pair);
    check(!dr.beat_dome, "the pair that won round 1 loses round 5");
    check_int(dr.threshold, 30, "because the threshold moved, not the hand");
}

static void test_cash_out_and_escape(void) {
    printf("dome: banking, escaping and busting\n");

    Dome d;
    dome_init(&d);
    d.phase = DOME_PHASE_CASHOUT;

    check_int(dome_cash_out(&d, 200), 200, "chips move to the bank");
    check_int(d.chips, 300, "off the stack");
    check_int(d.bank, 200, "and into the bank");
    check_int(dome_score(&d), 200, "the score is the bank alone");

    check_int(dome_cash_out(&d, 99999), 300, "banking more than you have banks it all");
    check_int(d.chips, 0, "leaving the stack empty");
    check_int(dome_cash_out(&d, 50), 0, "and there is nothing left to bank");
    check_int(dome_cash_out(&d, -10), 0, "a negative amount is ignored");

    /* Escape: everything left is banked and the session ends on your terms. */
    Dome esc;
    dome_init(&esc);
    esc.phase = DOME_PHASE_CASHOUT;
    esc.bank = 100;
    check(dome_can_escape(&esc), "escape is offered between rounds with chips in hand");
    dome_escape(&esc);
    check_int(esc.bank, 600, "escaping banks the whole stack");
    check_int(esc.chips, 0, "leaving nothing at risk");
    check(esc.session_over, "the session is over");
    check(esc.escaped, "and it ended by choice");
    check_int(dome_score(&esc), 600, "the score is everything banked");

    /* Escape is not offered mid-hand, nor with an empty stack. */
    Dome mid;
    dome_init(&mid);
    mid.phase = DOME_PHASE_BETTING;
    check(!dome_can_escape(&mid), "escape is not offered mid-hand");

    Dome broke;
    dome_init(&broke);
    broke.phase = DOME_PHASE_CASHOUT;
    broke.chips = 0;
    check(!dome_can_escape(&broke), "nor with nothing to walk away with");

    /* Bust: no chips and no bank is the end, and it is not an escape. */
    Dome bust;
    dome_init(&bust);
    bust.chips = 0;
    bust.bank = 0;
    check_int(dome_start_round(&bust), 0, "a session with nothing left cannot continue");
    check(bust.session_over, "it is over");
    check(!bust.escaped, "and it is a bust, not an escape");

    /* An empty stack but a filled bank is NOT bust at the round gate -- there is
       a score, and the ante is what will end it. */
    Dome banked;
    dome_init(&banked);
    banked.chips = 0;
    banked.bank = 400;
    check(dome_start_round(&banked), "an empty stack with a bank still starts a round");
    check(!banked.session_over, "so the session is not over yet");
    check_int(dome_charge_ante(&banked), 0, "but the ante ends it");
    check_int(dome_score(&banked), 400, "and the banked score survives");
}

/* A whole session, played out, so the pieces are checked composing rather than
 * only in isolation. The pressure is arithmetic: a fixed strategy that always
 * bets the minimum and never banks must eventually be ground down by the ante,
 * no matter how the cards fall. */
static void test_session_is_survivable_but_not_forever(void) {
    printf("dome: the ante grinds a passive player down\n");

    Dome d;
    dome_init(&d);

    HandResult pair;
    memset(&pair, 0, sizeof(pair));
    pair.rank = HAND_ONE_PAIR;
    pair.points = hand_points(HAND_ONE_PAIR);
    pair.valid = 1;

    int rounds = 0;
    while (!d.session_over && rounds < 500) {
        if (!dome_start_round(&d)) break;
        if (!dome_charge_ante(&d)) break;
        dome_place_bet(&d, dome_min_bet(&d));
        dome_resolve(&d, &pair);          /* wins early, loses later */
        rounds++;
    }

    check(d.session_over, "a player who never banks is eventually finished");
    check(rounds > 1, "but not immediately");
    check(rounds < 500, "and the loop terminated rather than running out");
    printf("  a minimum-betting, never-banking player lasted %d rounds\n", rounds);

    /* Banking instead of pressing keeps the score. */
    Dome careful;
    dome_init(&careful);
    dome_start_round(&careful);
    dome_charge_ante(&careful);
    dome_place_bet(&careful, dome_min_bet(&careful));
    dome_resolve(&careful, &pair);
    int before = careful.chips;
    dome_escape(&careful);
    check_int(dome_score(&careful), before, "walking away keeps exactly what was on the table");
}

/* The bank is not a one-way door. Without a way back, a player whose stack falls
 * below the minimum bet while holding a full bank reaches a betting screen with
 * no legal move -- money on the table, none of it reachable.
 */
static void test_digging_into_savings(void) {
    printf("dome: pulling chips back out of the bank\n");

    Dome d;
    dome_init(&d);
    d.chips = 3;
    d.bank  = 400;

    check(dome_must_dig_into_savings(&d),
          "a stack below the minimum with a bank is a dead end worth flagging");

    check_int(dome_withdraw_from_bank(&d, 100), 100, "chips come back out");
    check_int(d.chips, 103, "onto the table");
    check_int(d.bank, 300, "and out of the bank");
    check(!dome_must_dig_into_savings(&d), "which resolves the dead end");

    check_int(dome_withdraw_from_bank(&d, 99999), 300, "asking for more empties the bank");
    check_int(d.bank, 0, "leaving nothing banked");
    check_int(dome_withdraw_from_bank(&d, 50), 0, "and nothing further to fetch");
    check_int(dome_withdraw_from_bank(&d, -10), 0, "a negative amount is ignored");

    /* Not a dead end when there is nothing to fetch -- that is just bust. */
    Dome broke;
    dome_init(&broke);
    broke.chips = 2;
    broke.bank  = 0;
    check(!dome_must_dig_into_savings(&broke),
          "a small stack with an empty bank is not a dead end, it is the end");

    /* Nor once the session is over. */
    Dome done;
    dome_init(&done);
    done.chips = 1;
    done.bank  = 500;
    done.session_over = 1;
    check(!dome_must_dig_into_savings(&done), "a finished session is not offered a way out");

    /* Banking your whole stack must not be a trap.
     *
     * Found by playing, not by reading: a soak run banked everything, the next
     * round charged an ante it could not pay, and the session ended as a bust
     * holding 612 chips. Doing the safe thing was punished, which is the exact
     * opposite of what the bank is for. The ante must not be charged while this
     * is true. */
    Dome banked_out;
    dome_init(&banked_out);
    dome_start_round(&banked_out);
    dome_charge_ante(&banked_out);
    dome_cash_out(&banked_out, banked_out.chips);      /* bank it all */
    check_int(banked_out.chips, 0, "banking everything empties the stack");
    check(banked_out.bank > 0, "and fills the bank");

    check(dome_start_round(&banked_out), "the next round still starts");
    check(dome_needs_savings_for_ante(&banked_out),
          "and the player is short of the ante with money to fetch");
    check(!banked_out.session_over,
          "so the session is NOT over -- charging here would be the trap");

    dome_withdraw_from_bank(&banked_out, 100);
    check(dome_charge_ante(&banked_out), "after fetching chips the ante is payable");
    check(!banked_out.session_over, "and play continues");

    /* Genuinely empty is still the end -- the fix must not make bust unreachable. */
    Dome truly_broke;
    dome_init(&truly_broke);
    truly_broke.chips = 2;
    truly_broke.bank  = 0;
    truly_broke.round = 1;
    check(!dome_needs_savings_for_ante(&truly_broke),
          "an empty bank offers nothing to fetch");
    check_int(dome_charge_ante(&truly_broke), 0, "so the ante ends the session");

    /* Round trip: banking and unbanking the same chips must conserve them.
       This is the property that would catch a sign error in either direction. */
    Dome rt;
    dome_init(&rt);
    int before = rt.chips + rt.bank;
    dome_cash_out(&rt, 250);
    dome_withdraw_from_bank(&rt, 250);
    check_int(rt.chips + rt.bank, before, "chips are conserved across a round trip");
    check_int(rt.chips, STARTING_CHIPS, "and end up exactly where they started");
}

int main(void) {
    test_ante_schedule();
    test_threshold_curve();
    test_payouts();
    test_round_and_ante_charging();
    test_betting();
    test_resolution();
    test_cash_out_and_escape();
    test_digging_into_savings();
    test_session_is_survivable_but_not_forever();
    return report();
}
