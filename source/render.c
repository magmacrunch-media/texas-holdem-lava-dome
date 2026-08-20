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

void render_card(int x, int y, Card c, const Palette *p) {
    u32 ink = palette_pip_color(p, c.suit);

    ui_draw_panel(x, y, CARD_W, CARD_H, p->card_face, p->card_border, CARD_RADIUS);

    /* Rank in the corner with a small pip beneath it, the way the index on a
       real card reads. */
    draw_plain_text(x + 8, y + 8, card_rank_label(c.rank), 14, ink);
    render_pip(x + 15, y + 40, 15, c.suit, ink);

    /* The one that carries the suit across a room. */
    render_pip(x + CARD_W / 2, y + CARD_H / 2 + 12, 34, c.suit, ink);
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

static void render_header(const Dome *d, const Palette *p) {
    ui_draw_panel(PANEL_X, 8, PANEL_W, 34, p->panel_bg, p->panel_border, PANEL_RADIUS);

    char buf[32];

    snprintf(buf, sizeof(buf), "CHIPS %d", d->chips);
    ui_draw_text_shadow(PANEL_X + 14, 18, buf, 12, p->text);

    snprintf(buf, sizeof(buf), "BANK %d", d->bank);
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

void render_table(const Dome *d,
                  const Card *hole, int hole_count,
                  const Card *community, int community_count,
                  const HandResult *hand,
                  const Palette *p) {
    GRRLIB_FillScreen(p->table_bg);

    render_header(d, p);
    render_depth_bar(d, p);

    /* Five slots always, so the shape of a Hold'Em board is on screen from the
       first frame rather than assembling itself as the streets arrive. The
       labels name the street each one is waiting for. */
    static const char *STREET[5] = { "F", "F", "F", "T", "R" };
    for (int i = 0; i < 5; i++) {
        int x = render_community_x(i);
        if (i < community_count) {
            render_card(x, COMMUNITY_Y, community[i], p);
        } else {
            render_card_slot(x, COMMUNITY_Y, STREET[i], p);
        }
    }

    for (int i = 0; i < 2; i++) {
        int x = render_hole_x(i);
        if (i < hole_count) {
            render_card(x, HOLE_Y, hole[i], p);
        } else {
            render_card_slot(x, HOLE_Y, "?", p);
        }
    }

    if (hand && hand->valid) {
        ui_draw_centered_text(HOLE_Y + CARD_H + 10, hand_name(hand->rank), 14, p->text);
    }
}

/* --- the phase panel ------------------------------------------------------ */

#define OPTION_H   34
#define OPTION_GAP 10

void render_phase_panel(const char *title, const char *subtitle,
                        const char *const *options, int option_count,
                        int selected, const Palette *p) {
    ui_draw_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
                  p->panel_bg, p->panel_border, PANEL_RADIUS);

    if (title && *title) {
        ui_draw_centered_text(PANEL_Y + 12, title, 14, p->text);
    }
    if (subtitle && *subtitle) {
        ui_draw_centered_text(PANEL_Y + 38, subtitle, 10, p->text_dim);
    }

    if (option_count <= 0) return;

    /* Buttons share the panel's width evenly. Four all-in-one-row is the widest
       case the game asks for, and an even split keeps them the same size
       whether there are two or four. */
    int inner = PANEL_W - 28;
    int w = (inner - (option_count - 1) * OPTION_GAP) / option_count;
    int y = PANEL_Y + PANEL_H - OPTION_H - 16;

    for (int i = 0; i < option_count; i++) {
        int x = PANEL_X + 14 + i * (w + OPTION_GAP);
        int on = (i == selected);

        ui_draw_panel(x, y, w, OPTION_H,
                      on ? p->highlight : p->slot_bg,
                      on ? p->text : p->panel_border, 4);
        ui_draw_text_centered_in(x, y, w, OPTION_H, options[i], 11,
                                 on ? p->table_bg : p->text);
    }
}

void render_result_panel(const DomeResult *r, const Palette *p) {
    u32 tone = r->beat_dome ? p->win : p->loss;

    ui_draw_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, p->panel_bg, tone, PANEL_RADIUS);

    ui_draw_centered_text(PANEL_Y + 12,
                          r->beat_dome ? "BEAT THE DOME" : "THE DOME WINS", 16, tone);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s -- %d POINTS vs %d",
             hand_name(r->hand), r->points, r->threshold);
    ui_draw_centered_text(PANEL_Y + 44, buf, 10, p->text);

    if (r->beat_dome) {
        snprintf(buf, sizeof(buf), "+%d CHIPS", r->chips_won);
    } else {
        snprintf(buf, sizeof(buf), "-%d CHIPS", r->chips_lost);
    }
    ui_draw_centered_text(PANEL_Y + 70, buf, 14, tone);

    ui_draw_centered_text(PANEL_Y + 104, "A: CONTINUE", 10, p->text_dim);
}
