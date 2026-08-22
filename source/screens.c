#include <stdio.h>

#include "screens.h"
#include "dome.h"
#include "hand_eval.h"
#include "render.h"

/* Body text is deliberately wrapped rather than hand-broken: the safe area is a
   different width on PAL, and lines split by hand only look right on the set
   they were split for. */
#define BODY_SIZE 11
#define BODY_X     58
#define BODY_W    524

static const char *title_labels[TITLE_ITEM_COUNT] = {
    "ENTER THE DOME",
    "HOW TO PLAY",
    "HIGH SCORES",
    "CREDITS"
};

const char *screens_title_label(int item) {
    if (item < 0 || item >= TITLE_ITEM_COUNT) return "";
    return title_labels[item];
}

/* Every screen here opens the same way. Not renderer_draw_background(), which
   fills black: this game's ground is the lava palette's near-black, and the
   border is its own colour because magnolia's is hardcoded cyan. */
static void begin_screen(const Palette *p) {
    GRRLIB_FillScreen(p->table_bg);
    render_border(p);
}

void screens_draw_title(const Palette *p, const MenuGrid *menu) {
    begin_screen(p);

    ui_draw_centered_text(56,  "TEXAS HOLD'EM", 22, p->accent);
    ui_draw_centered_text(94,  "LAVA DOME", 30, p->highlight);
    ui_draw_centered_text(136, "SOLO POKER", 10, p->text_dim);

    for (int i = 0; i < TITLE_ITEM_COUNT; i++) {
        int y = 186 + i * 52;
        int selected = (i == menu->cursor);

        ui_draw_panel(190, y, 260, 40,
                      selected ? p->highlight : p->slot_bg,
                      selected ? p->text : p->panel_border, 6);

        /* Dark ink on the highlight, so no shadow -- see render.h. */
        if (selected) {
            render_text_centered_in(190, y, 260, 40, title_labels[i], 13, p->table_bg);
        } else {
            ui_draw_text_centered_in(190, y, 260, 40, title_labels[i], 13, p->text);
        }
    }

    ui_draw_centered_text(420, "UP/DOWN: CHOOSE   A: SELECT   HOME: QUIT", 10, p->text_dim);
}

void screens_draw_howto(const Palette *p, int page) {
    begin_screen(p);

    char head[40];
    snprintf(head, sizeof(head), "HOW TO PLAY  %d/%d", page + 1, HOWTO_PAGES);
    ui_draw_centered_text(28, head, 16, p->accent);

    int y = 74;
    switch (page) {
        case 0:
            ui_draw_text_shadow(BODY_X, y, "THE GOAL", 13, p->highlight);
            y += 30;
            y = ui_draw_text_wrapped(BODY_X, y, BODY_W,
                "Survive as long as you can inside the dome, and build your bank "
                "before it claims you. There is no opponent. Your hand is scored "
                "in points against a threshold that climbs every round.",
                BODY_SIZE, p->text_dim, 20);
            y += 14;

            ui_draw_text_shadow(BODY_X, y, "EACH ROUND", 13, p->highlight);
            y += 30;
            y = ui_draw_text_wrapped(BODY_X, y, BODY_W,
                "The dome charges an ante. You get two cards and place a bet. "
                "The flop, turn and river come one at a time, and at each one "
                "you can check, raise, or fold to cut your losses.",
                BODY_SIZE, p->text_dim, 20);
            break;

        case 1:
            ui_draw_text_shadow(BODY_X, y, "THE BANK", 13, p->highlight);
            y += 30;
            y = ui_draw_text_wrapped(BODY_X, y, BODY_W,
                "After each round you can move chips into your bank, where they "
                "cannot be lost. Your score is what you bank, and only what you "
                "bank -- chips still on the table were never yours.",
                BODY_SIZE, p->text_dim, 20);
            y += 14;
            y = ui_draw_text_wrapped(BODY_X, y, BODY_W,
                "You can pull chips back out again. Banking is safe, not final.",
                BODY_SIZE, p->text, 20);
            y += 14;

            ui_draw_text_shadow(BODY_X, y, "ESCAPE OR BUST", 13, p->highlight);
            y += 30;
            y = ui_draw_text_wrapped(BODY_X, y, BODY_W,
                "Escape and you bank everything and walk out. Go bust and the "
                "dome keeps whatever was on the table. The ante climbs forever, "
                "so knowing when to leave is the whole game.",
                BODY_SIZE, p->text_dim, 20);
            break;

        default: {
            ui_draw_text_shadow(BODY_X, y, "HAND VALUES", 13, p->highlight);
            y += 28;

            /* Read out of the tables the game is actually balanced on, best
               hand first, so this page cannot drift from them. */
            for (int r = HAND_CLASS_COUNT - 1; r >= 0; r--) {
                int pay = dome_payout_hundredths((HandRank)r);

                char line[64];
                if (pay == 0) {
                    snprintf(line, sizeof(line), "%-16s  %4d PTS   --",
                             hand_name((HandRank)r), hand_points((HandRank)r));
                } else {
                    /* Hundredths spelled out, because a flush pays 2.5x and
                       printing it as 2 would misstate the payout by a fifth. */
                    snprintf(line, sizeof(line), "%-16s  %4d PTS   %d.%02dx",
                             hand_name((HandRank)r), hand_points((HandRank)r),
                             pay / 100, pay % 100);
                }
                ui_draw_text_shadow(BODY_X + 10, y, line, 10, p->text);
                y += 22;
            }

            y += 6;
            ui_draw_text_wrapped(BODY_X, y, BODY_W,
                "High card pays nothing and scores nothing, so it cannot beat "
                "the dome at any depth.",
                10, p->text_dim, 18);
            break;
        }
    }

    ui_draw_centered_text(444, "LEFT/RIGHT: PAGE    B: BACK", 10, p->text_dim);
}

void screens_draw_credits(const Palette *p) {
    begin_screen(p);

    ui_draw_centered_text(34, "CREDITS", 18, p->accent);

    int y = 84;
    y = ui_draw_text_wrapped(BODY_X, y, BODY_W,
        "A solo card game inspired by Texas hold'em poker, Poker Squares, and "
        "the band Texas Hold'Em Lava Dome.",
        BODY_SIZE, p->text_dim, 20);
    y += 20;

    ui_draw_text_shadow(BODY_X, y, "GAME DESIGN & DEVELOPMENT", 11, p->highlight);
    y += 26;
    ui_draw_text_shadow(BODY_X + 10, y, "Jake A. McCoy", 11, p->text);
    y += 34;

    ui_draw_text_shadow(BODY_X, y, "THIS PORT", 11, p->highlight);
    y += 26;
    ui_draw_text_shadow(BODY_X + 10, y, "magnolia, a Wii homebrew engine", 10, p->text);
    y += 22;
    ui_draw_text_shadow(BODY_X + 10, y, "Press Start 2P by CodeMan38", 10, p->text);
    y += 22;
    ui_draw_text_shadow(BODY_X + 10, y, "GRRLIB and devkitPPC", 10, p->text);
    y += 34;

    ui_draw_text_shadow(BODY_X, y, "PUBLISHER", 11, p->highlight);
    y += 26;
    ui_draw_text_shadow(BODY_X + 10, y, "MagmaCrunch Media", 11, p->text);

    ui_draw_centered_text(444, "B: BACK", 10, p->text_dim);
}
