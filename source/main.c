#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "magnolia.h"
#include "config.h"
#include "cards.h"
#include "dealer.h"
#include "dome.h"
#include "hand_eval.h"
#include "palette.h"
#include "render.h"

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

static const Palette *pal;

/* The choice on offer in the current phase. Labels are built rather than
   constant because the bet amounts come from the stack and change every round. */
/* Wide enough for the longest label with the largest chip count that fits an
   int -- "BANK 2147483647" plus its terminator. A stack that big is not
   reachable, but a buffer sized for the reachable case is the kind of thing
   that stops being true when someone retunes the payouts. */
#define MAX_OPTIONS 4
static char        option_text[MAX_OPTIONS][20];
static const char *option_labels[MAX_OPTIONS];
static int         option_values[MAX_OPTIONS];
static int         option_count;

/* Which set is currently loaded. Tracked explicitly rather than inferred from
   option_count, which was the first version and was wrong: the bet options
   deduplicate down to two on a short stack, so "there are two options" does not
   mean "these are the savings options" -- it would have shown bet labels
   attached to withdraw amounts, at exactly the moment the player is short of
   chips and least able to afford the confusion. */
typedef enum {
    OPT_NONE,
    OPT_BET,
    OPT_CASHOUT,
    OPT_SAVINGS
} OptionSet;

static OptionSet option_set;

static DomeResult last_result;
static HandResult current_hand;

/* A session ends two ways and they are not the same event. Busting is the dome
   taking everything; escaping is walking out with the bank. Both produce a
   score, so both reach the leaderboard -- but only one is a defeat, and the
   game-over screen says which. */
static int session_escaped;

/* Every choice in this game is A, so this is the only input the loop reads for
   confirmation -- and the only one autoplay has to synthesise. Routing every
   site through here is what makes an unattended session possible at all. */
static int press_a(void) {
    if (input_a_pressed()) return 1;

#if DEBUG_AUTOPLAY_FRAMES
    if (clock_frame() > 0 && clock_frame() % DEBUG_AUTOPLAY_FRAMES == 0) {
        /* Move the cursor before confirming. Without this a soak run picks the
           first button every time, which means it never banks, never escapes,
           and never scores -- so the whole tail of the game, including the
           leaderboard, goes unexercised while the log looks healthy. */
        if (option_count > 0) {
            menu_grid_set_cursor(&options,
                                 (clock_frame() / DEBUG_AUTOPLAY_FRAMES) % option_count);
        }
        return 1;
    }
#endif
    return 0;
}

static void set_options(int n, OptionSet which) {
    option_count = n;
    option_set   = which;
    for (int i = 0; i < n; i++) option_labels[i] = option_text[i];
    menu_grid_init(&options, n, n, 1);   /* one row: a horizontal button strip */
}

/* Quick-pick bet sizes: minimum, a quarter, a half, all-in -- deduplicated,
   because on a short stack several of those collapse onto the same number and
   offering the same amount twice reads as a bug. */
static void build_bet_options(void) {
    int chips = dome.chips;
    int lo    = dome_min_bet(&dome);

    int candidates[MAX_OPTIONS];
    candidates[0] = lo;
    candidates[1] = chips / 4 > lo ? chips / 4 : lo;
    candidates[2] = chips / 2 > lo ? chips / 2 : lo;
    candidates[3] = chips;

    int n = 0;
    for (int i = 0; i < MAX_OPTIONS; i++) {
        int dup = 0;
        for (int j = 0; j < n; j++) if (option_values[j] == candidates[i]) dup = 1;
        if (dup) continue;

        option_values[n] = candidates[i];
        if (candidates[i] == chips && chips > lo) {
            snprintf(option_text[n], sizeof(option_text[n]), "ALL IN");
        } else {
            snprintf(option_text[n], sizeof(option_text[n]), "%d", candidates[i]);
        }
        n++;
    }
    set_options(n, OPT_BET);
}

static void build_cashout_options(void) {
    int chips = dome.chips;

    option_values[0] = 0;
    snprintf(option_text[0], sizeof(option_text[0]), "PRESS ON");
    option_values[1] = chips / 2;
    snprintf(option_text[1], sizeof(option_text[1]), "BANK %d", chips / 2);
    option_values[2] = chips;
    snprintf(option_text[2], sizeof(option_text[2]), "BANK ALL");
    option_values[3] = -1;
    snprintf(option_text[3], sizeof(option_text[3]), "ESCAPE");

    set_options(4, OPT_CASHOUT);
}

static void build_savings_options(void) {
    int bank = dome.bank;

    option_values[0] = bank / 4 > MIN_BET ? bank / 4 : bank;
    option_values[1] = bank;
    snprintf(option_text[0], sizeof(option_text[0]), "TAKE %d", option_values[0]);
    snprintf(option_text[1], sizeof(option_text[1]), "TAKE ALL");
    set_options(2, OPT_SAVINGS);
}

/* The ante has been paid; deal in. */
static void deal_in(void) {
    dealer_start_round(&dealer, &rng);
    current_hand = hand_eval_partial(dealer.hole, dealer.hole_count);
    build_bet_options();
}

static void begin_round(void) {
    /* Clear the board first. The savings prompt below returns before any cards
       are dealt, and without this the player would be asked to fetch chips while
       last round's hand -- the one they just lost with -- is still face up. */
    memset(&dealer, 0, sizeof(dealer));
    memset(&current_hand, 0, sizeof(current_hand));

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
    scoring_reset();
    memset(&last_result, 0, sizeof(last_result));
    begin_round();

    printf("session: start, %d chips\n", dome.chips);
}

/* Advances the board one street and re-reads what the player is holding. */
static void advance_street(void) {
    switch (dome.phase) {
        case DOME_PHASE_BETTING: dealer_flop(&dealer);  dome.phase = DOME_PHASE_FLOP;  break;
        case DOME_PHASE_FLOP:    dealer_turn(&dealer);  dome.phase = DOME_PHASE_TURN;  break;
        case DOME_PHASE_TURN:    dealer_river(&dealer); dome.phase = DOME_PHASE_RIVER; break;
        default: return;
    }

    Card all[HAND_MAX_CARDS];
    int n = dealer_all_cards(&dealer, all);
    current_hand = (n >= 5) ? hand_eval(all, n) : hand_eval_partial(all, n);
}

static void resolve_round(void) {
    Card all[HAND_MAX_CARDS];
    int n = dealer_all_cards(&dealer, all);
    current_hand = hand_eval(all, n);

    last_result = dome_resolve(&dome, &current_hand);
    printf("round %d: %s, %d vs %d, %s\n",
           dome.round, hand_name(current_hand.rank),
           current_hand.points, last_result.threshold,
           last_result.beat_dome ? "won" : "lost");
}

/* One frame of a session. */
static void update_session(GameStateMachine *gs) {
    if (input_dir_repeat(INPUT_DIR_LEFT))  menu_grid_move(&options, -1, 0);
    if (input_dir_repeat(INPUT_DIR_RIGHT)) menu_grid_move(&options, +1, 0);

    render_table(&dome, dealer.hole, dealer.hole_count,
                 dealer.community, dealer.community_count, &current_hand, pal);

    char sub[64];

    /* A stack too short to bet, with chips in the bank, is a dead end unless the
       player is offered the way back out. Checked before the phases below,
       because it can be true the moment a betting round opens. */
    if (dome.phase == DOME_PHASE_BETTING && dome_must_dig_into_savings(&dome)) {
        if (option_set != OPT_SAVINGS) build_savings_options();

        snprintf(sub, sizeof(sub), "%d CHIPS LEFT, %d BANKED", dome.chips, dome.bank);
        render_phase_panel("NOT ENOUGH TO BET", sub,
                           option_labels, option_count, options.cursor, pal);

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
            render_phase_panel("DIG INTO YOUR SAVINGS?", sub,
                               option_labels, option_count, options.cursor, pal);

            if (press_a()) {
                dome_withdraw_from_bank(&dome, option_values[options.cursor]);

                /* Still short even after emptying the bank: there is genuinely
                   nothing left, and the ante takes the last of it. */
                if (!dome_charge_ante(&dome)) {
                    printf("session: bust after %d rounds, %d banked\n",
                           dome.round, dome_score(&dome));
                    gamestate_end_run(gs, dome_score(&dome));
                } else {
                    deal_in();
                }
            }
            break;
        }

        case DOME_PHASE_BETTING:
            snprintf(sub, sizeof(sub), "%d CHIPS  -  BEAT %d POINTS",
                     dome.chips, dome_threshold_for_round(dome.round));
            render_phase_panel("PLACE YOUR BET", sub,
                               option_labels, option_count, options.cursor, pal);
            if (press_a()) {
                dome_place_bet(&dome, option_values[options.cursor]);
                advance_street();
            }
            break;

        case DOME_PHASE_FLOP:
        case DOME_PHASE_TURN:
        case DOME_PHASE_RIVER: {
            static const char *NEXT[] = { "A: SEE THE TURN", "A: SEE THE RIVER", "A: SHOWDOWN" };
            int which = (int)dome.phase - (int)DOME_PHASE_FLOP;

            snprintf(sub, sizeof(sub), "BET %d  -  BEAT %d POINTS",
                     dome.current_bet, dome_threshold_for_round(dome.round));
            render_phase_panel(hand_name(current_hand.rank), sub, NULL, 0, 0, pal);
            ui_draw_centered_text(PANEL_Y + PANEL_H - 40, NEXT[which], 12, pal->highlight);

            if (press_a()) {
                if (dome.phase == DOME_PHASE_RIVER) resolve_round();
                else                                advance_street();
            }
            break;
        }

        case DOME_PHASE_CASHOUT:
            render_result_panel(&last_result, pal);
            if (press_a()) {
                /* Busting on the resolve leaves nothing to decide. */
                if (dome.session_over) {
                    printf("session: bust after %d rounds, %d banked\n",
                           dome.round, dome_score(&dome));
                    gamestate_end_run(gs, dome_score(&dome));
                } else {
                    build_cashout_options();
                    dome.phase = DOME_PHASE_IDLE;
                }
            }
            break;

        case DOME_PHASE_IDLE: {
            snprintf(sub, sizeof(sub), "%d ON THE TABLE  -  %d BANKED",
                     dome.chips, dome.bank);
            render_phase_panel("BANK IT, OR PRESS ON?", sub,
                               option_labels, option_count, options.cursor, pal);

            if (press_a()) {
                int v = option_values[options.cursor];

                if (v < 0) {
                    dome_escape(&dome);
                    session_escaped = 1;
                    printf("session: escaped with %d banked\n", dome_score(&dome));
                    gamestate_end_run(gs, dome_score(&dome));
                    break;
                }

                if (v > 0) dome_cash_out(&dome, v);

                begin_round();
                if (dome.session_over) {
                    printf("session: bust after %d rounds, %d banked\n",
                           dome.round, dome_score(&dome));
                    gamestate_end_run(gs, dome_score(&dome));
                }
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

/* --- the screens the shell does not draw --------------------------------- */

static void draw_title(void) {
    GRRLIB_FillScreen(pal->table_bg);
    render_border(pal);

    ui_draw_centered_text(84,  "TEXAS HOLD'EM", 24, pal->accent);
    ui_draw_centered_text(126, "LAVA DOME", 32, pal->highlight);

    ui_draw_centered_text(210, "The dome charges an ante every round.", 10, pal->text_dim);
    ui_draw_centered_text(232, "Beat its threshold and win chips.", 10, pal->text_dim);
    ui_draw_centered_text(254, "Bank them, or press your luck.", 10, pal->text_dim);
    ui_draw_centered_text(284, "Your score is what you bank.", 10, pal->text);

    ui_draw_centered_text(360, "A: ENTER THE DOME", 14, pal->text);
    ui_draw_centered_text(404, "HOME: QUIT", 10, pal->text_dim);
    renderer_finish();
}

/* Drawn here rather than taken from the shell. magnolia's game over assumes a
   run that ended in failure -- A sends you straight into another one. A player
   who chose to walk away wants the title screen, not a session they did not ask
   for. One game wanting that is not two, so it stays here until a second game
   needs the same thing. */
static void draw_game_over(const GameStateMachine *gs) {
    GRRLIB_FillScreen(pal->table_bg);
    render_border(pal);

    u32 tone = session_escaped ? pal->win : pal->loss;
    ui_draw_centered_text(88, session_escaped ? "YOU ESCAPED THE DOME"
                                              : "THE DOME KEEPS YOU", 18, tone);
    ui_draw_centered_text(124, dome_depth_label(dome.round), 10, pal->text_dim);

    char buf[48];
    snprintf(buf, sizeof(buf), "BANKED %d", dome_score(&dome));
    ui_draw_centered_text(176, buf, 24, pal->text);

    snprintf(buf, sizeof(buf), "AFTER %d ROUND%s", dome.round, dome.round == 1 ? "" : "S");
    ui_draw_centered_text(216, buf, 11, pal->text_dim);

    if (gs->is_high_score) {
        snprintf(buf, sizeof(buf), "HIGH SCORE  -  RANK %d", gs->rank);
        ui_draw_centered_text(268, buf, 14, pal->highlight);
        ui_draw_centered_text(392, "A: ENTER INITIALS", 11, pal->text);
    } else {
        ui_draw_centered_text(392, "A: BACK TO THE TITLE", 11, pal->text);
    }
    renderer_finish();
}

static void draw_initials(const GameStateMachine *gs) {
    GRRLIB_FillScreen(pal->table_bg);
    render_border(pal);

    ui_draw_centered_text(96, "NEW HIGH SCORE", 20, pal->highlight);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", dome_score(&dome));
    ui_draw_centered_text(142, buf, 26, pal->text);

    for (int i = 0; i < 3; i++) {
        char ch[2] = { gs->initials[i], '\0' };
        int x = 250 + i * 50;
        int active = (i == gs->cursor_pos);
        ui_draw_panel(x, 210, 40, 52,
                      active ? pal->highlight : pal->slot_bg, pal->panel_border, 4);
        ui_draw_text_centered_in(x, 210, 40, 52, ch, 24,
                                 active ? pal->table_bg : pal->text);
    }

    ui_draw_centered_text(300, "LEFT/RIGHT: LETTER", 10, pal->text_dim);
    ui_draw_centered_text(322, "DOWN: NEXT    A: DONE", 10, pal->text_dim);
    renderer_finish();
}

static void draw_high_scores(void) {
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

    ui_draw_centered_text(420, "A: BACK", 10, pal->text_dim);
    renderer_finish();
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
                draw_title();
                if (press_a()) {
                    start_session();
                    gamestate_set(&gs, GS_PLAYING);
                }
                continue;

            case GS_GAME_OVER:
                draw_game_over(&gs);
                if (press_a()) {
                    if (gs.is_high_score) gamestate_begin_initials(&gs);
                    else                  gamestate_set(&gs, GS_TITLE);
                }
                continue;

            case GS_INITIALS:
                draw_initials(&gs);
                break;

            case GS_HIGH_SCORES:
                draw_high_scores();
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
