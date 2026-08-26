#include <stdio.h>
#include <string.h>
#include "magnolia.h"
#include "render.h"

#define CARD_RADIUS   6
#define PANEL_RADIUS  8

/* Five community slots and two hole cards, both rows centred in the design
   space, so the arithmetic lives here once instead of at every draw site. */
static int row_x(int index, int count) {
    int span = count * CARD_W + (count - 1) * CARD_GAP;
    int left = (UI_DESIGN_WIDTH - span) / 2;
    return left + index * (CARD_W + CARD_GAP);
}

int render_community_x(int index) { return row_x(index, 5); }
int render_hole_x(int index)      { return row_x(index, 2); }

/* --- pips ---------------------------------------------------------------
 *
 * The one place this game draws with GRRLIB rather than through ui_utils.
 * Coordinates arrive in design units and are mapped here, so a pip scales with
 * the safe area exactly like the card it sits on.
 */

static void fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3, u32 color) {
    guVector v[3];
    u32 c[3] = { color, color, color };

    v[0].x = (f32)ui_map_x(x1); v[0].y = (f32)ui_map_y(y1); v[0].z = 0.0f;
    v[1].x = (f32)ui_map_x(x2); v[1].y = (f32)ui_map_y(y2); v[1].z = 0.0f;
    v[2].x = (f32)ui_map_x(x3); v[2].y = (f32)ui_map_y(y3); v[2].z = 0.0f;

    GRRLIB_NGoneFilled(v, c, 3);
}

static void fill_circle(int cx, int cy, int r, u32 color) {
    int mapped = ui_map_w(r);
    if (mapped < 1) mapped = 1;   /* a radius that rounds to zero draws nothing */
    GRRLIB_Circle((f32)ui_map_x(cx), (f32)ui_map_y(cy), (f32)mapped, color, true);
}

static void fill_rect(int x, int y, int w, int h, u32 color) {
    GRRLIB_Rectangle((f32)ui_map_x(x), (f32)ui_map_y(y),
                     (f32)ui_map_w(w), (f32)ui_map_h(h), color, true);
}

void render_pip(int cx, int cy, int size, int suit, u32 color) {
    /* A quarter of the width is the unit every shape is built from, which keeps
       the four suits visually the same weight at any size. */
    int r = size / 4;
    if (r < 2) r = 2;

    if (suit == SUIT_DIAMONDS) {
        /* Convex, so a single triangle fan is exact rather than approximate.
           Drawn as two triangles about the centre for the same reason a fan of
           four points would work -- this way the shape is unambiguous. */
        fill_triangle(cx, cy - 2 * r, cx + 2 * r, cy, cx, cy + 2 * r, color);
        fill_triangle(cx, cy - 2 * r, cx - 2 * r, cy, cx, cy + 2 * r, color);
        return;
    }

    if (suit == SUIT_HEARTS) {
        fill_circle(cx - r, cy - r, r, color);
        fill_circle(cx + r, cy - r, r, color);
        fill_triangle(cx - 2 * r, cy - r, cx + 2 * r, cy - r, cx, cy + 2 * r, color);
        return;
    }

    if (suit == SUIT_SPADES) {
        /* A heart upside down, with a stem. */
        fill_triangle(cx, cy - 2 * r, cx - 2 * r, cy + r / 2, cx + 2 * r, cy + r / 2, color);
        fill_circle(cx - r, cy + r / 2, r, color);
        fill_circle(cx + r, cy + r / 2, r, color);
        fill_rect(cx - r / 3, cy + r, (r / 3) * 2 + 1, r, color);
        return;
    }

    /* Clubs: three lobes and a stem. Slightly smaller radius so the trefoil
       does not outgrow the other three suits. */
    int cr = (size * 2) / 9;
    if (cr < 2) cr = 2;
    fill_circle(cx, cy - cr, cr, color);
    fill_circle(cx - cr, cy + cr / 2, cr, color);
    fill_circle(cx + cr, cy + cr / 2, cr, color);
    fill_rect(cx - cr / 3, cy + cr / 2, (cr / 3) * 2 + 1, cr + cr / 2, color);
}

/* --- cards --------------------------------------------------------------- */

/* Text with no drop shadow.
 *
 * Every text helper in magnolia's ui_utils draws a hardcoded black shadow two
 * pixels down and right. That is right for the games it was written for, which
 * put light text on dark backgrounds -- and wrong on a playing card, where a
 * black shadow under a red rank turns a Q into a smudge. Verified on a
 * screenshot before it was believed: the pips came out clean and the ranks came
 * out mush.
 *
 * A shadow colour argument would fix this for everyone, but this is the first
 * game to want it and one consumer is not two, so it stays here. Recorded in
 * the README as the engine's first real candidate from this port. */
static void draw_plain_text(int design_x, int design_y, const char *text,
                            unsigned int design_size, u32 color) {
    if (!ttf_font || !text) return;
    GRRLIB_PrintfTTF(ui_map_x(design_x), ui_map_y(design_y), ttf_font, text,
                     ui_map_size(design_size), color);
}

void render_text_centered_in(int design_x, int design_y,
                             int design_w, int design_h,
                             const char *text, unsigned int design_size,
                             u32 color) {
    if (!ttf_font || !text) return;

    /* Same arithmetic as ui_draw_text_centered_in(), minus the shadow pass. The
       width has to be measured at the mapped size rather than the design size,
       or the centring drifts on any video mode that is not 640x480. */
    unsigned int size = ui_map_size(design_size);
    u32 tw = GRRLIB_WidthTTF(ttf_font, text, size);

    int x = ui_map_x(design_x) + (ui_map_w(design_w) - (int)tw) / 2;
    int y = ui_map_y(design_y) + (ui_map_h(design_h) - (int)size) / 2;

    GRRLIB_PrintfTTF(x, y, ttf_font, text, size, color);
}

u32 render_fade(u32 color, float alpha) {
    if (alpha >= 1.0f) return color;
    if (alpha <= 0.0f) return color & 0xFFFFFF00u;

    unsigned a = (unsigned)((float)(color & 0xFFu) * alpha + 0.5f);
    if (a > 0xFFu) a = 0xFFu;
    return (color & 0xFFFFFF00u) | a;
}

static void draw_card_face(int x, int y, Card c, const Palette *p, float alpha) {
    u32 ink = render_fade(palette_pip_color(p, c.suit), alpha);

    ui_draw_panel(x, y, CARD_W, CARD_H,
                  render_fade(p->card_face, alpha),
                  render_fade(p->card_border, alpha), CARD_RADIUS);

    /* Rank in the corner with a small pip beneath it, the way the index on a
       real card reads. */
    draw_plain_text(x + 8, y + 8, card_rank_label(c.rank), 14, ink);
    render_pip(x + 15, y + 40, 15, c.suit, ink);

    /* The one that carries the suit across a room. */
    render_pip(x + CARD_W / 2, y + CARD_H / 2 + 12, 34, c.suit, ink);
}

void render_card(int x, int y, Card c, const Palette *p) {
    draw_card_face(x, y, c, p, 1.0f);
}

/* How far above its slot a card starts, and how long it takes to land. */
#define REVEAL_RISE  26

/* A card arriving: it falls the last few pixels into its slot and fades up.
 *
 * It slides rather than scales, and that is not only for the arithmetic. Press
 * Start 2P is a pixel font, and scaling a card means drawing its rank at
 * fractional sizes on every frame of the animation -- which is the same smudge
 * the drop-shadow workaround above exists to avoid, arriving by a different
 * route. Sliding leaves every glyph at the one size it was designed for.
 *
 * Eased out, so the card is quickest when it is furthest away and settles
 * gently. A linear slide reads as a card being pushed; this reads as one being
 * dealt. */
static void render_card_revealing(int x, int y, Card c, const Palette *p, float t) {
    if (t >= 1.0f) {
        render_card(x, y, c, p);
        return;
    }
    if (t < 0.0f) t = 0.0f;

    float e = ease_out_quad(t);
    int   dy = (int)((1.0f - e) * (float)REVEAL_RISE);

    draw_card_face(x, y - dy, c, p, e);
}

void render_border(const Palette *p) {
    /* Screen pixels, not design units: the border traces the safe area itself,
       which ui_safe_* already reports directly. */
    int x = ui_safe_x(), y = ui_safe_y();
    int w = ui_safe_w(), h = ui_safe_h();
    const int t = 3;

    GRRLIB_Rectangle((f32)x, (f32)y, (f32)w, (f32)t, p->panel_border, true);
    GRRLIB_Rectangle((f32)x, (f32)(y + h - t), (f32)w, (f32)t, p->panel_border, true);
    GRRLIB_Rectangle((f32)x, (f32)y, (f32)t, (f32)h, p->panel_border, true);
    GRRLIB_Rectangle((f32)(x + w - t), (f32)y, (f32)t, (f32)h, p->panel_border, true);
}

void render_card_slot(int x, int y, const char *label, const Palette *p) {
    ui_draw_panel(x, y, CARD_W, CARD_H, p->slot_bg, p->slot_border, CARD_RADIUS);
    if (label && *label) {
        ui_draw_text_centered_in(x, y, CARD_W, CARD_H, label, 18, p->text_dim);
    }
}

/* --- the table ----------------------------------------------------------- */

static void render_header(const Dome *d, int shown_chips, int shown_bank,
                          const Palette *p) {
    ui_draw_panel(PANEL_X, 8, PANEL_W, 34, p->panel_bg, p->panel_border, PANEL_RADIUS);

    char buf[32];

    snprintf(buf, sizeof(buf), "CHIPS %d", shown_chips);
    ui_draw_text_shadow(PANEL_X + 14, 18, buf, 12, p->text);

    snprintf(buf, sizeof(buf), "BANK %d", shown_bank);
    ui_draw_text_shadow(PANEL_X + 190, 18, buf, 12, p->win);

    snprintf(buf, sizeof(buf), "R%d", d->round);
    ui_draw_text_shadow(PANEL_X + 360, 18, buf, 12, p->text_dim);

    snprintf(buf, sizeof(buf), "ANTE %d", dome_ante_for_round(d->round));
    ui_draw_text_shadow(PANEL_X + 430, 18, buf, 12, p->accent);
}

static void render_depth_bar(const Dome *d, const Palette *p) {
    ui_draw_centered_text(50, dome_depth_label(d->round), 10, p->text_dim);

    /* The warning is the whole pacing of the game made visible: the ante is
       about to cost more than it does now, and that is the information the
       cash-out decision turns on. */
    int next = dome_ante_for_round(d->round + 1);
    if (next > dome_ante_for_round(d->round)) {
        char buf[48];
        snprintf(buf, sizeof(buf), "! ANTE RISES TO %d NEXT ROUND", next);
        ui_draw_centered_text(66, buf, 10, p->accent);
    }
}

/* Where the player stands, on screen at all times.
 *
 * The hand name alone was not enough. Everything in this game is a decision
 * about a number -- points against a threshold -- and a player who has to
 * remember what the threshold was, or work out whether a straight clears it,
 * is being asked to do the game's arithmetic in their head. The web build
 * carries the same line for the same reason.
 *
 * No arrows on the badge. Press Start 2P has no triangle glyphs, and a missing
 * glyph on a CRT is indistinguishable from a bug -- the same reason the pips
 * above are drawn rather than typed. */
static void render_hand_status(const Dome *d, const HandResult *hand,
                               int card_count, const Palette *p) {
    if (!hand || !hand->valid) return;

    ui_draw_centered_text(288, hand_name(hand->rank), 13, p->text);

    /* Points only once there is a five-card hand to have any. Below that the
       evaluator is reading pairs off two or four cards and its points would be
       a promise the board has not made yet. */
    if (card_count < 5) {
        ui_draw_centered_text(306, "STILL COMING", 10, p->text_dim);
        return;
    }

    int threshold = dome_threshold_for_round(d->round);
    int beating   = hand->points >= threshold;

    char line[64];
    snprintf(line, sizeof(line), "%d PTS vs %d  -  %s",
             hand->points, threshold, beating ? "BEATING" : "LOSING");
    ui_draw_centered_text(306, line, 10, beating ? p->win : p->loss);
}

void render_table(const Dome *d,
                  int shown_chips, int shown_bank,
                  const Card *hole, int hole_count,
                  const Card *community, int community_count,
                  int reveal_from, float reveal_t,
                  const HandResult *hand,
                  const Palette *p) {
    GRRLIB_FillScreen(p->table_bg);

    render_header(d, shown_chips, shown_bank, p);
    render_depth_bar(d, p);

    /* Five slots always, so the shape of a Hold'Em board is on screen from the
       first frame rather than assembling itself as the streets arrive. The
       labels name the street each one is waiting for. */
    static const char *STREET[5] = { "F", "F", "F", "T", "R" };
    for (int i = 0; i < 5; i++) {
        int x = render_community_x(i);
        if (i >= community_count) {
            render_card_slot(x, COMMUNITY_Y, STREET[i], p);
        } else if (reveal_from >= 0 && i >= reveal_from) {
            render_card_revealing(x, COMMUNITY_Y, community[i], p, reveal_t);
        } else {
            render_card(x, COMMUNITY_Y, community[i], p);
        }
    }

    for (int i = 0; i < 2; i++) {
        int x = render_hole_x(i);
        if (i >= hole_count) {
            render_card_slot(x, HOLE_Y, "?", p);
        } else if (reveal_from == RENDER_REVEAL_HOLE) {
            render_card_revealing(x, HOLE_Y, hole[i], p, reveal_t);
        } else {
            render_card(x, HOLE_Y, hole[i], p);
        }
    }

    render_hand_status(d, hand, hole_count + community_count, p);
}

/* --- the phase panel ------------------------------------------------------ */

#define OPTION_H   32
#define OPTION_GAP 10
#define OPTION_ROW_GAP 6

void render_phase_panel(const char *title,
                        const char *subtitle, u32 subtitle_color,
                        const char *const *options, int option_count, int cols,
                        int selected,
                        const char *note, u32 note_color,
                        const Palette *p) {
    ui_draw_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
                  p->panel_bg, p->panel_border, PANEL_RADIUS);

    if (title && *title) {
        ui_draw_centered_text(PANEL_Y + 10, title, 14, p->text);
    }
    if (subtitle && *subtitle) {
        ui_draw_centered_text(PANEL_Y + 36, subtitle, 10, subtitle_color);
    }

    if (option_count <= 0) return;

    if (cols < 1) cols = option_count;
    if (cols > OPTION_COLS_MAX) cols = OPTION_COLS_MAX;

    int rows = (option_count + cols - 1) / cols;
    if (rows > OPTION_ROWS_MAX) rows = OPTION_ROWS_MAX;

    /* Columns share the panel's width evenly and every row uses the same
       column positions, so a button does not shift sideways because the row
       below it happens to be short. That matters more than it sounds: index i
       sits at row i / cols and column i % cols, which is exactly where
       magnolia's MenuGrid believes the cursor is. */
    int inner = PANEL_W - 28;
    int w = (inner - (cols - 1) * OPTION_GAP) / cols;

    int last_row_y = PANEL_Y + PANEL_H - OPTION_H - 16;
    int top_row_y  = last_row_y - (rows - 1) * (OPTION_H + OPTION_ROW_GAP);

    /* The note lives where the second row of buttons would be, so it is only
       drawn when there is no second row. Callers with a two-row grid pass NULL;
       this is the belt to that braces. */
    if (rows == 1 && note && *note) {
        ui_draw_centered_text(PANEL_Y + 70, note, 10, note_color);
    }

    for (int i = 0; i < option_count; i++) {
        int r = i / cols;
        int c = i % cols;
        if (r >= rows) break;

        int x = PANEL_X + 14 + c * (w + OPTION_GAP);
        int y = top_row_y + r * (OPTION_H + OPTION_ROW_GAP);
        int on = (i == selected);

        ui_draw_panel(x, y, w, OPTION_H,
                      on ? p->highlight : p->slot_bg,
                      on ? p->text : p->panel_border, 4);

        /* The highlighted button is dark ink on the palette's yellow, so it
           takes the shadowless draw; the rest are light on dark, where the
           shadow is doing its job. */
        if (on) {
            render_text_centered_in(x, y, w, OPTION_H, options[i], 11, p->table_bg);
        } else {
            ui_draw_text_centered_in(x, y, w, OPTION_H, options[i], 11, p->text);
        }
    }
}

void render_result_panel(const DomeResult *r, float pulse_t, const Palette *p) {
    /* A fold is neither a win nor a defeat on the merits, and colouring it like
       a loss would say the dome out-played a hand nobody looked at. It gets the
       panel's own border and the dimmer voice. */
    u32 tone = r->folded ? p->text_dim : (r->beat_dome ? p->win : p->loss);

    /* The outline fades up as the panel arrives. It is the one part of the
       result that changes colour with the outcome, so it is the part worth
       drawing attention to -- and a fade does that without a flash, which on a
       lava-red screen at CRT brightness is worth avoiding. */
    if (pulse_t < 0.0f) pulse_t = 0.0f;
    if (pulse_t > 1.0f) pulse_t = 1.0f;
    u32 outline = render_fade(tone, 0.35f + 0.65f * ease_out_quad(pulse_t));

    ui_draw_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, p->panel_bg, outline, PANEL_RADIUS);

    const char *banner = r->folded    ? "YOU FOLDED"
                       : r->beat_dome ? "BEAT THE DOME"
                                      : "THE DOME WINS";
    ui_draw_centered_text(PANEL_Y + 10, banner, 16, tone);

    char buf[64];
    if (r->folded) {
        /* No hand line: none was evaluated, and printing HIGH CARD here would
           be the game inventing a showdown that never happened. */
        snprintf(buf, sizeof(buf), "THE BOARD MISSED YOU  -  %d NEEDED", r->threshold);
    } else {
        snprintf(buf, sizeof(buf), "%s -- %d POINTS vs %d",
                 hand_name(r->hand), r->points, r->threshold);
    }
    ui_draw_centered_text(PANEL_Y + 38, buf, 10, p->text);

    if (r->beat_dome) {
        snprintf(buf, sizeof(buf), "+%d CHIPS", r->chips_won);
    } else {
        snprintf(buf, sizeof(buf), "-%d CHIPS", r->chips_lost);
    }
    ui_draw_centered_text(PANEL_Y + 62, buf, 14, tone);

    /* Wrapped rather than centred on one line: the longest quip is 56
       characters, which at a size anyone can read across a room is wider than
       the panel. Wrapping puts the decision about where it breaks in one place
       instead of in the length of each string. */
    if (r->flavor && *r->flavor) {
        ui_draw_text_wrapped(PANEL_X + 24, PANEL_Y + 86, PANEL_W - 48,
                             r->flavor, 9, p->text_dim, 13);
    }

    ui_draw_centered_text(PANEL_Y + 118, "A: CONTINUE", 9, p->text_dim);
}
