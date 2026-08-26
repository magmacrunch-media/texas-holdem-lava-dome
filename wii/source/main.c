#include <gccore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "magnolia.h"
#include "anim.h"
#include "config.h"
#include "cards.h"
#include "dealer.h"
#include "dome.h"
#include "hand_eval.h"
#include "palette.h"
#include "render.h"
#include "screens.h"

/* Texas Hold'Em Lava Dome.
 *
 * Two state machines, and they do not overlap. magnolia's GameStateMachine owns
 * the shell -- title, the session, game over, initials, leaderboard -- and one
 * whole session lives inside GS_PLAYING. Dome's own phase says where inside that
 * session you are: betting, watching a street, or deciding what to do with the
 * chips you just won.
 *
 * The score is the bank and only the bank. Chips still on the table were never
 * yours, which is the entire tension of the game.
 */

static Dome     dome;
static Dealer   dealer;
static Rng      rng;
static MenuGrid options;
static MenuGrid title_menu;

static const Palette *pal;

/* --- the choice on offer ---------------------------------------------------
 *
 * Labels are built rather than constant because the amounts come from the stack
 * and change every round.
 */

/* Eight is two rows of four, which is what the cash-out screen needs: banking
   and digging into savings are both decisions the player can make at the same
   moment, and offering only one of them at a time was how a full bank became a
   trap. Labels are wide enough for the longest one with the largest chip count
   that fits an int -- "BANK 2147483647" plus its terminator. A stack that big is
   not reachable, but a buffer sized for the reachable case is the kind of thing
   that stops being true when someone retunes the payouts. */
#define MAX_OPTIONS 8

/* What pressing A on an option means.
 *
 * The kind is tracked beside the value rather than encoded into it. Encoding was
 * the first attempt and it does not survive contact: banking 200 and fetching
 * 200 are different buttons carrying the same number, so the value alone cannot
 * tell them apart, and a sentinel small enough to be tidy is a sentinel a real
 * one-chip withdrawal can collide with. */
typedef enum {
    OPTION_AMOUNT,      /* chips to commit or to bank */
    OPTION_WITHDRAW,    /* chips to fetch back out of the bank */
    OPTION_ACTION       /* a decision, named by the ACT_* values below */
} OptionKind;

#define ACT_PRESS_ON  1
#define ACT_ESCAPE    2
#define ACT_CHECK     3
#define ACT_FOLD      4

static char        option_text[MAX_OPTIONS][20];
static const char *option_labels[MAX_OPTIONS];
static int         option_values[MAX_OPTIONS];
static OptionKind  option_kinds[MAX_OPTIONS];
static int         option_count;
static int         option_cols;

/* Which set is currently loaded. Tracked explicitly rather than inferred from
   option_count, which was the first version and was wrong: the bet options
   deduplicate down to two on a short stack, so "there are two options" does not
   mean "these are the savings options" -- it would have shown bet labels
   attached to withdraw amounts, at exactly the moment the player is short of
   chips and least able to afford the confusion. */
typedef enum {
    OPT_NONE,
    OPT_BET,
    OPT_STREET,
    OPT_CASHOUT,
    OPT_SAVINGS
} OptionSet;

static OptionSet option_set;

static DomeResult last_result;
static HandResult current_hand;

/* --- what is moving on screen ---------------------------------------------
 *
 * All of it is advanced in one place, once a frame, and handed to the renderer
 * as plain numbers. Nothing in render.c reads the clock.
 */

/* Long enough to read as a card being dealt, short enough that four of them do
   not turn a hand into a cutscene. */
#define REVEAL_SECONDS  0.22f
#define PULSE_SECONDS   0.40f

static Tween chips_shown;
static Tween bank_shown;
static float reveal_t     = 1.0f;
static int   reveal_from  = RENDER_REVEAL_NONE;
static float result_pulse = 1.0f;

/* A session ends two ways and they are not the same event. Busting is the dome
   taking everything; escaping is walking out with the bank. Both produce a
   score, so both reach the leaderboard -- but only one is a defeat, and the
   game-over screen says which. */
static int         session_escaped;
static const char *end_flavor;

/* The title screen's own state. Not magnolia's GS_MENU: that is for choosing
   something before a run -- a mode, a character -- and this game has nothing to
   choose. These are screens you read and back out of, so they hang off the
   title the way george-boole's do. */
typedef enum {
    OVERLAY_NONE,
    OVERLAY_HOWTO,
    OVERLAY_SCORES,
    OVERLAY_CREDITS
} Overlay;

static Overlay overlay;
static int     howto_page;

/* --- input ---------------------------------------------------------------- */

/* Whether cards are still arriving. Confirming into a half-dealt board would
   resolve a hand the player has not seen. */
static int revealing(void) {
    return reveal_t < 1.0f;
}

/* Every choice in this game is A, so this is the only input the loop reads for
   confirmation -- and the only one autoplay has to synthesise. Routing every
   site through here is what makes an unattended session possible at all. */
static int press_a(void) {
#if DEBUG_AUTOPLAY_FRAMES
    if (clock_frame() > 0 && clock_frame() % DEBUG_AUTOPLAY_FRAMES == 0) {
        /* Deliberately ahead of the reveal gate below. A synthetic press lands
           on a frame number rather than on a moment somebody chose, so it can
           fall inside a deal -- and a soak run that silently drops a press per
           street is a soak run that has stopped soaking. A real player cannot
           press through a reveal; this is not a real player.

           Move the cursor before confirming, and move it at random.

           Without any movement a soak picks the first button every time, so it
           never banks, never escapes and never scores -- the whole tail of the
           game, leaderboard included, goes unexercised while the log looks
           healthy. Stepping the cursor by the press number instead was the next
           attempt, and it fails in a way that is harder to see: the press
           number advances in lockstep with the phases, so the same index lands
           on the same button in every session. Once the streets grew a
           four-button menu that index was FOLD, every hand -- a soak that
           looked busy folded six hands out of six and never once reached a
           showdown.

           Drawing the cursor costs the shuffle its reproducibility from a seed,
           which is a real property of this game. It costs it only in a build
           that is playing itself, and coverage is the entire point of that
           build. */
        if (option_count > 0) {
            menu_grid_set_cursor(&options, rng_below(&rng, option_count));
        }
        return 1;
    }
#endif
    if (revealing()) return 0;
    return input_a_pressed();
}

/* --- building the options -------------------------------------------------- */

static void begin_options(OptionSet which) {
    option_count = 0;
    option_set   = which;
}

/* Adds an option, refusing a duplicate.
 *
 * Two options of the same kind carrying the same number is the case worth
 * stopping: on a short stack a quarter, a half and the whole lot collapse onto
 * one amount, and offering it three times reads as a bug at exactly the moment
 * the player is least able to afford the confusion. */
static int add_option(OptionKind kind, int value, const char *fmt, ...) {
    if (option_count >= MAX_OPTIONS) return 0;
    if (kind != OPTION_ACTION && value <= 0) return 0;

    for (int i = 0; i < option_count; i++) {
        if (option_kinds[i] == kind && option_values[i] == value) return 0;
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(option_text[option_count], sizeof(option_text[0]), fmt, ap);
    va_end(ap);

    option_kinds[option_count]  = kind;
    option_values[option_count] = value;
    option_count++;
    return 1;
}

static void end_options(int cols) {
    if (option_count <= 0) {
        option_cols = 1;
        menu_grid_init(&options, 0, 1, 1);
        return;
    }

    if (cols < 1 || cols > option_count) cols = option_count;
    if (cols > OPTION_COLS_MAX) cols = OPTION_COLS_MAX;

    int rows = (option_count + cols - 1) / cols;
    if (rows > OPTION_ROWS_MAX) rows = OPTION_ROWS_MAX;

    option_cols = cols;
    for (int i = 0; i < option_count; i++) option_labels[i] = option_text[i];
    menu_grid_init(&options, option_count, cols, rows);
}

/* Quick-pick bet sizes: minimum, a quarter, a half, all-in. */
static void build_bet_options(void) {
    int chips = dome.chips;
    int lo    = dome_min_bet(&dome);

    begin_options(OPT_BET);
    add_option(OPTION_AMOUNT, lo, "%d", lo);
    add_option(OPTION_AMOUNT, chips / 4, "%d", chips / 4);
    add_option(OPTION_AMOUNT, chips / 2, "%d", chips / 2);
    if (chips > lo) add_option(OPTION_AMOUNT, chips, "ALL IN");
    else            add_option(OPTION_AMOUNT, chips, "%d", chips);
    end_options(option_count);
}

/* Check, raise, or cut your losses -- the decision the arcade version offers at
 * every street and this port used to skip.
 *
 * A raise also turns the next card, which is js/ui.js's behaviour and not an
 * accident of it: raising and staying on the same street would be a second
 * betting loop the web version does not have, and four decisions in a hand is
 * already the shape of the game. */
static void build_street_options(void) {
    int chips = dome.chips;
    int lo    = dome_min_bet(&dome);

    static const char *NEXT[] = { "TURN", "RIVER", "SHOWDOWN" };
    int which = (int)dome.phase - (int)DOME_PHASE_FLOP;
    if (which < 0) which = 0;
    if (which > 2) which = 2;

    begin_options(OPT_STREET);
    add_option(OPTION_ACTION, ACT_CHECK, "%s", NEXT[which]);
    if (dome_can_raise(&dome)) {
        add_option(OPTION_AMOUNT, lo, "+%d", lo);
        add_option(OPTION_AMOUNT, chips / 4, "+%d", chips / 4);
    }
    add_option(OPTION_ACTION, ACT_FOLD, "FOLD");
    end_options(option_count);
}

/* The between-rounds decision, and everything that belongs to it.
 *
 * Two rows, because banking and pulling back out are the same decision seen
 * from opposite sides and the player should not have to reach a dead end before
 * the second half of it is offered. The web build puts both on one screen; this
 * port used to offer a withdrawal only once the stack was already too small to
 * act with, which is help arriving after it was needed. */
static void build_cashout_options(void) {
    int chips = dome.chips;
    int bank  = dome.bank;

    begin_options(OPT_CASHOUT);

    add_option(OPTION_AMOUNT, chips / 4, "BANK %d", chips / 4);
    add_option(OPTION_AMOUNT, chips / 2, "BANK %d", chips / 2);
    add_option(OPTION_AMOUNT, (chips * 3) / 4, "BANK %d", (chips * 3) / 4);
    add_option(OPTION_AMOUNT, chips, "BANK ALL");

    add_option(OPTION_ACTION, ACT_PRESS_ON, "PRESS ON");
    if (dome_can_escape(&dome)) add_option(OPTION_ACTION, ACT_ESCAPE, "ESCAPE");

    add_option(OPTION_WITHDRAW, bank / 2, "TAKE %d", bank / 2);
    add_option(OPTION_WITHDRAW, bank, "TAKE ALL");

    end_options(OPTION_COLS_MAX);
}

/* The forced version: the player is short and the only way on is through the
   bank. Amounts only -- there is nothing else to decide here. */
static void build_savings_options(void) {
    int bank = dome.bank;

    begin_options(OPT_SAVINGS);
    add_option(OPTION_WITHDRAW, bank / 4, "TAKE %d", bank / 4);
    add_option(OPTION_WITHDRAW, bank / 2, "TAKE %d", bank / 2);
    add_option(OPTION_WITHDRAW, bank, "TAKE ALL");
    end_options(option_count);
}

/* --- the round ------------------------------------------------------------- */

static void begin_reveal(int from) {
    reveal_from = from;
    reveal_t    = 0.0f;
}

/* The ante has been paid; deal in. */
static void deal_in(void) {
    dealer_start_round(&dealer, &rng);
    current_hand = hand_eval_partial(dealer.hole, dealer.hole_count);
    begin_reveal(RENDER_REVEAL_HOLE);
    build_bet_options();
}

static void begin_round(void) {
    /* Clear the board first. The savings prompt below returns before any cards
       are dealt, and without this the player would be asked to fetch chips while
       last round's hand -- the one they just lost with -- is still face up. */
    memset(&dealer, 0, sizeof(dealer));
    memset(&current_hand, 0, sizeof(current_hand));
    reveal_from = RENDER_REVEAL_NONE;
    reveal_t    = 1.0f;

    if (!dome_start_round(&dome)) return;

    /* Do not charge an ante the player cannot pay while their money is sitting
       in the bank. Banking your whole stack is otherwise a trap: you do the safe
       thing and the next ante ends the session, holding a full bank. Let them
       fetch chips first, then charge. */
    if (dome_needs_savings_for_ante(&dome)) {
        dome.phase = DOME_PHASE_SAVINGS;
        build_savings_options();
        return;
    }

    if (!dome_charge_ante(&dome)) return;
    deal_in();
}

static void start_session(void) {
    dome_init(&dome);
    session_escaped = 0;
    end_flavor      = NULL;
    scoring_reset();
    memset(&last_result, 0, sizeof(last_result));

    /* Snapped, not tweened. Counting up from the last run's total would animate
       a number that was never real. */
    tween_set(&chips_shown, (float)dome.chips);
    tween_set(&bank_shown,  (float)dome.bank);

    begin_round();

    printf("session: start, %d chips\n", dome.chips);
}

/* Advances the board one street and re-reads what the player is holding. */
static void advance_street(void) {
    int before = dealer.community_count;

    switch (dome.phase) {
        case DOME_PHASE_BETTING: dealer_flop(&dealer);  dome.phase = DOME_PHASE_FLOP;  break;
        case DOME_PHASE_FLOP:    dealer_turn(&dealer);  dome.phase = DOME_PHASE_TURN;  break;
        case DOME_PHASE_TURN:    dealer_river(&dealer); dome.phase = DOME_PHASE_RIVER; break;
        default: return;
    }

    begin_reveal(before);

    Card all[HAND_MAX_CARDS];
    int n = dealer_all_cards(&dealer, all);
    current_hand = (n >= 5) ? hand_eval(all, n) : hand_eval_partial(all, n);

    /* Rebuilt here rather than on entry to the phase: the button that turns the
       next card is named after it, and a raise has just moved the stack the
       raise amounts are computed from. */
    build_street_options();
}

static void enter_cashout(void) {
    result_pulse = 0.0f;
    build_cashout_options();
}

static void resolve_round(void) {
    Card all[HAND_MAX_CARDS];
    int n = dealer_all_cards(&dealer, all);
    current_hand = hand_eval(all, n);

    last_result = dome_resolve(&dome, &current_hand, &rng);
    enter_cashout();

    printf("round %d: %s, %d vs %d, %s\n",
           dome.round, hand_name(current_hand.rank),
           current_hand.points, last_result.threshold,
           last_result.beat_dome ? "won" : "lost");
}

static void fold_round(void) {
    last_result = dome_fold(&dome, &rng);
    enter_cashout();

    printf("round %d: folded, %d lost\n", dome.round, last_result.chips_lost);
}

static void end_session(GameStateMachine *gs, int escaped) {
    session_escaped = escaped;
    end_flavor      = escaped ? dome_flavor_escape(&rng) : dome_flavor_bust(&rng);

    /* Clear any deal still in flight. press_a() refuses while cards are
       arriving, and reveal_t is only advanced by update_session() -- which the
       shell stops calling the moment the run ends. A session that ended part
       way through a reveal would otherwise leave the game-over screen unable
       to accept the one button it offers. */
    reveal_t    = 1.0f;
    reveal_from = RENDER_REVEAL_NONE;

    printf("session: %s after %d rounds, %d banked\n",
           escaped ? "escaped" : "bust", dome.round, dome_score(&dome));

    gamestate_end_run(gs, dome_score(&dome));
}

/* --- drawing the session --------------------------------------------------- */

static u32 risk_color(RiskLevel r) {
    switch (r) {
        case RISK_ALL_IN: return pal->loss;
        case RISK_HIGH:   return pal->risk_high;
        case RISK_MEDIUM: return pal->accent;
        default:          return pal->risk_low;
    }
}

/* What the highlighted button will actually do, spelled out.
 *
 * The buttons are four across and therefore terse -- "+120" has to fit in a
 * quarter of the panel. This line is where the terseness is paid back, and it
 * is where the risk badge lives, so a player sizing a bet reads the consequence
 * in the same place they read the amount. */
static const char *option_note(u32 *color) {
    static char note[80];

    if (option_count <= 0) return NULL;

    OptionKind kind = option_kinds[options.cursor];
    int        v    = option_values[options.cursor];

    if (kind == OPTION_AMOUNT && (option_set == OPT_BET || option_set == OPT_STREET)) {
        RiskLevel r = dome_risk_level(&dome, v);
        *color = risk_color(r);
        snprintf(note, sizeof(note), "%s %d  -  %s",
                 option_set == OPT_BET ? "BET" : "RAISE", v, dome_risk_label(r));
        return note;
    }

    if (option_set == OPT_STREET && kind == OPTION_ACTION) {
        *color = pal->text_dim;
        if (v == ACT_FOLD) {
            snprintf(note, sizeof(note), "GIVE UP THE HAND, KEEP %d CHIPS", dome.chips);
        } else {
            snprintf(note, sizeof(note), "SEE THE NEXT CARD WITHOUT RAISING");
        }
        return note;
    }

    if (option_set == OPT_SAVINGS) {
        *color = pal->text_dim;
        snprintf(note, sizeof(note), "%d IN THE BANK", dome.bank);
        return note;
    }

    return NULL;
}

/* One frame of a session. */
static void update_session(GameStateMachine *gs) {
    float dt = clock_dt();

    /* The cursor stays live during a reveal. Only confirming is held back --
       a highlight that froze as well would read as the game hanging. */
    if (input_dir_repeat(INPUT_DIR_LEFT))  menu_grid_move(&options, -1, 0);
    if (input_dir_repeat(INPUT_DIR_RIGHT)) menu_grid_move(&options, +1, 0);
    if (option_count > option_cols) {
        if (input_dir_repeat(INPUT_DIR_UP))   menu_grid_move(&options, 0, -1);
        if (input_dir_repeat(INPUT_DIR_DOWN)) menu_grid_move(&options, 0, +1);
    }

    anim_step(&reveal_t, dt, REVEAL_SECONDS);
    anim_step(&result_pulse, dt, PULSE_SECONDS);

    tween_to(&chips_shown, (float)dome.chips);
    tween_to(&bank_shown,  (float)dome.bank);
    tween_step(&chips_shown, dt);
    tween_step(&bank_shown,  dt);

    render_table(&dome, tween_display(&chips_shown), tween_display(&bank_shown),
                 dealer.hole, dealer.hole_count,
                 dealer.community, dealer.community_count,
                 reveal_from, reveal_t, &current_hand, pal);

    char sub[64];
    u32  note_color = pal->text_dim;
    const char *note = option_note(&note_color);

    /* A stack too short to bet, with chips in the bank, is a dead end unless the
       player is offered the way back out. Checked before the phases below,
       because it can be true the moment a betting round opens. */
    if (dome.phase == DOME_PHASE_BETTING && dome_must_dig_into_savings(&dome)) {
        if (option_set != OPT_SAVINGS) build_savings_options();

        snprintf(sub, sizeof(sub), "%d CHIPS LEFT, %d BANKED", dome.chips, dome.bank);
        render_phase_panel("NOT ENOUGH TO BET", sub, pal->accent,
                           option_labels, option_count, option_cols, options.cursor,
                           NULL, pal->text_dim, pal);

        if (press_a()) {
            dome_withdraw_from_bank(&dome, option_values[options.cursor]);
            build_bet_options();
        }
        renderer_finish();
        return;
    }

    switch (dome.phase) {
        case DOME_PHASE_SAVINGS: {
            int ante = dome_ante_for_round(dome.round);
            if (option_set != OPT_SAVINGS) build_savings_options();

            snprintf(sub, sizeof(sub), "ANTE IS %d, YOU HAVE %d  -  %d BANKED",
                     ante, dome.chips, dome.bank);
            render_phase_panel("DIG INTO YOUR SAVINGS?", sub, pal->accent,
                               option_labels, option_count, option_cols, options.cursor,
                               note, note_color, pal);

            if (press_a()) {
                dome_withdraw_from_bank(&dome, option_values[options.cursor]);

                /* Still short even after emptying the bank: there is genuinely
                   nothing left, and the ante takes the last of it. */
                if (!dome_charge_ante(&dome)) end_session(gs, 0);
                else                          deal_in();
            }
            break;
        }

        case DOME_PHASE_BETTING:
            snprintf(sub, sizeof(sub), "%d CHIPS  -  BEAT %d POINTS",
                     dome.chips, dome_threshold_for_round(dome.round));
            render_phase_panel("PLACE YOUR BET", sub, pal->text_dim,
                               option_labels, option_count, option_cols, options.cursor,
                               note, note_color, pal);
            if (press_a()) {
                dome_place_bet(&dome, option_values[options.cursor]);
                advance_street();
            }
            break;

        case DOME_PHASE_FLOP:
        case DOME_PHASE_TURN:
        case DOME_PHASE_RIVER: {
            if (option_set != OPT_STREET) build_street_options();

            snprintf(sub, sizeof(sub), "BET %d  -  BEAT %d POINTS",
                     dome.current_bet, dome_threshold_for_round(dome.round));
            render_phase_panel(hand_name(current_hand.rank), sub, pal->text_dim,
                               option_labels, option_count, option_cols, options.cursor,
                               note, note_color, pal);

            if (press_a()) {
                OptionKind kind = option_kinds[options.cursor];
                int        v    = option_values[options.cursor];

                if (kind == OPTION_ACTION && v == ACT_FOLD) {
                    fold_round();
                    break;
                }

                /* A raise buys the next card as well as a bigger stake -- the
                   arcade version's behaviour, and the reason a hand holds four
                   decisions rather than an open betting loop. */
                if (kind == OPTION_AMOUNT) dome_place_bet(&dome, v);

                if (dome.phase == DOME_PHASE_RIVER) resolve_round();
                else                                advance_street();
            }
            break;
        }

        case DOME_PHASE_CASHOUT:
            render_result_panel(&last_result, result_pulse, pal);
            if (press_a()) {
                /* Busting on the resolve leaves nothing to decide. */
                if (dome.session_over) end_session(gs, 0);
                else                   dome.phase = DOME_PHASE_IDLE;
            }
            break;

        case DOME_PHASE_IDLE: {
            if (option_set != OPT_CASHOUT) build_cashout_options();

            /* The header already carries both totals, so this line spends itself
               on what the header cannot say: what the next round costs, and
               whether the stack still covers it. That is the information this
               decision actually turns on, and the game used to give no warning
               at all before the ante that ends a run. */
            u32 sub_color = pal->text_dim;
            if (dome_ante_danger(&dome)) {
                snprintf(sub, sizeof(sub), "NEXT ANTE IS %d, YOU HAVE %d",
                         dome_next_ante(&dome), dome.chips);
                sub_color = pal->accent;
            } else {
                snprintf(sub, sizeof(sub), "NEXT ANTE IS %d", dome_next_ante(&dome));
            }

            render_phase_panel("BANK IT, OR PRESS ON?", sub, sub_color,
                               option_labels, option_count, option_cols, options.cursor,
                               NULL, pal->text_dim, pal);

            if (press_a()) {
                OptionKind kind = option_kinds[options.cursor];
                int        v    = option_values[options.cursor];

                if (kind == OPTION_ACTION && v == ACT_ESCAPE) {
                    dome_escape(&dome);
                    end_session(gs, 1);
                    break;
                }

                if (kind == OPTION_ACTION && v == ACT_PRESS_ON) {
                    begin_round();
                    if (dome.session_over) end_session(gs, 0);
                    break;
                }

                /* Moving chips is not the same decision as starting the next
                   round. Rolling them together is what made banking feel like a
                   commitment it never was -- and it is why the arcade version
                   has a separate NEXT ROUND button. */
                if (kind == OPTION_AMOUNT) dome_cash_out(&dome, v);
                else                       dome_withdraw_from_bank(&dome, v);

                build_cashout_options();
            }
            break;
        }
    }

#if DEBUG_HEARTBEAT_FRAMES
    if (clock_frame() % DEBUG_HEARTBEAT_FRAMES == 0) {
        printf("heartbeat: frame=%d round=%d phase=%d chips=%d bank=%d\n",
               clock_frame(), dome.round, (int)dome.phase, dome.chips, dome.bank);
    }
#endif

    renderer_finish();
}

/* --- the screens the shell does not draw ---------------------------------
 *
 * None of these flip. The title has an overlay that may still be waiting to be
 * drawn over it, and a screen that flips for itself cannot be composed.
 */

/* Drawn here rather than taken from the shell. magnolia's game over assumes a
   run that ended in failure -- A sends you straight into another one. A player
   who chose to walk away wants the title screen, not a session they did not ask
   for. One game wanting that is not two, so it stays here until a second game
   needs the same thing. */
static void draw_game_over(const GameStateMachine *gs) {
    GRRLIB_FillScreen(pal->table_bg);
    render_border(pal);

    u32 tone = session_escaped ? pal->win : pal->loss;
    ui_draw_centered_text(72, session_escaped ? "YOU ESCAPED THE DOME"
                                              : "THE DOME KEEPS YOU", 18, tone);
    ui_draw_centered_text(108, dome_depth_label(dome.round), 10, pal->text_dim);

    char buf[48];
    snprintf(buf, sizeof(buf), "BANKED %d", dome_score(&dome));
    ui_draw_centered_text(152, buf, 24, pal->text);

    snprintf(buf, sizeof(buf), "AFTER %d ROUND%s", dome.round, dome.round == 1 ? "" : "S");
    ui_draw_centered_text(192, buf, 11, pal->text_dim);

    if (end_flavor) {
        ui_draw_text_wrapped(90, 232, 460, end_flavor, 10, pal->text_dim, 18);
    }

    if (gs->is_high_score) {
        snprintf(buf, sizeof(buf), "HIGH SCORE  -  RANK %d", gs->rank);
        ui_draw_centered_text(310, buf, 14, pal->highlight);
        ui_draw_centered_text(392, "A: ENTER INITIALS", 11, pal->text);
    } else {
        ui_draw_centered_text(392, "A: BACK TO THE TITLE", 11, pal->text);
    }
}

static void draw_initials(const GameStateMachine *gs) {
    GRRLIB_FillScreen(pal->table_bg);
    render_border(pal);

    ui_draw_centered_text(96, "NEW HIGH SCORE", 20, pal->highlight);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", dome_score(&dome));
    ui_draw_centered_text(142, buf, 26, pal->text);

    for (int i = 0; i < 3; i++) {
        char ch[2] = { gs->initials[i], 0 };
        int x = 250 + i * 50;
        int active = (i == gs->cursor_pos);
        ui_draw_panel(x, 210, 40, 52,
                      active ? pal->highlight : pal->slot_bg, pal->panel_border, 4);

        /* A letter is one large glyph, so the smudge is at its worst here. */
        if (active) {
            render_text_centered_in(x, 210, 40, 52, ch, 24, pal->table_bg);
        } else {
            ui_draw_text_centered_in(x, 210, 40, 52, ch, 24, pal->text);
        }
    }

    ui_draw_centered_text(300, "LEFT/RIGHT: LETTER", 10, pal->text_dim);
    ui_draw_centered_text(322, "DOWN: NEXT    A: DONE", 10, pal->text_dim);
}

/* One leaderboard, two ways in: the shell reaches it after a run, and the title
   menu reaches it whenever anyone is curious. Only the footer differs, so only
   the footer is a parameter. */
static void draw_high_scores(const char *footer) {
    GRRLIB_FillScreen(pal->table_bg);
    render_border(pal);

    ui_draw_centered_text(46, "DEEPEST BANKS", 20, pal->accent);

    int count = scoring_get_count();
    if (count == 0) {
        ui_draw_centered_text(200, "NOBODY HAS ESCAPED YET", 12, pal->text_dim);
    }
    for (int i = 0; i < count && i < 10; i++) {
        const ScoreEntry *e = scoring_get_entry(i);
        if (!e) continue;

        char line[48];
        snprintf(line, sizeof(line), "%2d  %s  %d", i + 1, e->initials, e->score);
        ui_draw_centered_text(90 + i * 28, line, 13,
                              i == 0 ? pal->highlight : pal->text);
    }

    ui_draw_centered_text(420, footer, 10, pal->text_dim);
}

/* --- the title, and the screens behind it ---------------------------------- */

/* Returns 1 when the player has chosen to start a run.
 *
 * The title is driven here rather than by magnolia's shell, for the reason
 * george-boole gives: the shell reads A as "start", and here A means whichever
 * of four things is under the cursor. */
static int update_title(void) {
    if (overlay != OVERLAY_NONE) {
        switch (overlay) {
            case OVERLAY_HOWTO:  screens_draw_howto(pal, howto_page); break;
            case OVERLAY_SCORES: draw_high_scores("B: BACK");         break;
            default:             screens_draw_credits(pal);           break;
        }
        renderer_finish();

        if (overlay == OVERLAY_HOWTO) {
            if (input_dir_repeat(INPUT_DIR_LEFT)  && howto_page > 0) howto_page--;
            if (input_dir_repeat(INPUT_DIR_RIGHT) && howto_page < HOWTO_PAGES - 1) howto_page++;
        }

        /* B backs out, and so does A. On a screen with nothing to confirm,
           insisting on the one button that means "no" is how a player ends up
           stuck on the credits. */
        if (input_back_pressed() || input_a_pressed()) overlay = OVERLAY_NONE;
        return 0;
    }

    screens_draw_title(pal, &title_menu);
    renderer_finish();

    if (input_dir_repeat(INPUT_DIR_UP))   menu_grid_move(&title_menu, 0, -1);
    if (input_dir_repeat(INPUT_DIR_DOWN)) menu_grid_move(&title_menu, 0, +1);

    /* press_a() rather than input_a_pressed(), so an autoplay build starts the
       next session by itself. Without this the soak plays one run and stops on
       the title with a healthy-looking log -- which is the failure this hook
       exists to catch, arriving in the one place nobody watches. The cursor
       sits on ENTER THE DOME and autoplay never moves it, so the menu costs the
       soak nothing. */
    if (!press_a()) return 0;

    switch (title_menu.cursor) {
        case TITLE_HOWTO:   overlay = OVERLAY_HOWTO; howto_page = 0; return 0;
        case TITLE_SCORES:  overlay = OVERLAY_SCORES; return 0;
        case TITLE_CREDITS: overlay = OVERLAY_CREDITS; return 0;
        default:            return 1;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* printf reaches Dolphin's log through the OSReport channel -- but only if
       OSREPORT and WriteToFile are enabled in its Logger.ini. Both default to
       False, which makes a tracing session look like a silent one. Unbuffered,
       or the line written immediately before a crash dies with it. */
    SYS_STDIO_Report(true);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== texas hold'em lava dome starting ===\n");

    const MagnoliaConfig cfg = {
        .app_name     = APP_NAME,
        .max_scores   = HIGH_SCORE_COUNT,
        .overscan_pct = OVERSCAN_PCT
    };
    if (magnolia_init(&cfg) == -2) return 1;

    input_init();
    pal = palette_lava();

    /* Seeded from the clock once; every shuffle after that comes from the
       game's own generator, so a session is reproducible from its seed. */
    rng_seed(&rng, (unsigned int)time(NULL));

    menu_grid_init(&title_menu, TITLE_ITEM_COUNT, 1, TITLE_ITEM_COUNT);
    tween_set(&chips_shown, 0.0f);
    tween_set(&bank_shown,  0.0f);

    GameStateMachine gs;
    gamestate_init(&gs);

#if AUTOSTART_GAMEPLAY
    printf("autostart: skipping the title, dealing in\n");
    start_session();
    gamestate_set(&gs, GS_PLAYING);
#endif

    while (1) {
        input_scan();
        if (input_home_pressed()) break;

        if (gamestate_current(&gs) == GS_PLAYING) {
            update_session(&gs);
            continue;
        }

        switch (gamestate_current(&gs)) {
            case GS_TITLE:
                if (update_title()) {
                    start_session();
                    gamestate_set(&gs, GS_PLAYING);
                }
                continue;

            case GS_GAME_OVER:
                draw_game_over(&gs);
                renderer_finish();
                if (press_a()) {
#if DEBUG_AUTOPLAY_FRAMES
                    /* The initials editor and the leaderboard are driven by the
                       shell, which reads the real Wiimote -- there is no press
                       to synthesise into them. Every soak run scores, and every
                       score is a high score on a card `make dolphin` has just
                       cleared, so without this the soak plays exactly one
                       session and then sits on the initials screen forever with
                       a log that looks perfectly healthy.

                       Filing the run as AAA and going straight back to the
                       title is what makes it a soak. It also means the run
                       passes through scoring_add_entry() and the save, which is
                       the part of the tail that most deserves the exercise. */
                    if (gs.is_high_score) {
                        gamestate_begin_initials(&gs);
                        gamestate_commit_initials(&gs, dome_score(&dome));
                    }
                    gamestate_set(&gs, GS_TITLE);
#else
                    if (gs.is_high_score) gamestate_begin_initials(&gs);
                    else                  gamestate_set(&gs, GS_TITLE);
#endif
                }
                continue;

            case GS_INITIALS:
                draw_initials(&gs);
                renderer_finish();
                break;

            case GS_HIGH_SCORES:
                draw_high_scores("A: BACK");
                renderer_finish();
                break;

            default:
                gamestate_set(&gs, GS_TITLE);
                continue;
        }

        gamestate_update(&gs, dome_score(&dome));
    }

    magnolia_shutdown();
    return 0;
}
