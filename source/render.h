#ifndef RENDER_H
#define RENDER_H

#include "cards.h"
#include "dome.h"
#include "hand_eval.h"
#include "palette.h"

/* Drawing the table.
 *
 * Everything is authored in magnolia's fixed 640x480 design space and mapped
 * into the TV-safe area by the ui_map_* helpers, so the layout does not care
 * which video mode is running or how much of the picture the set eats.
 *
 * Almost all of it goes through magnolia's ui_utils -- panels, centred text,
 * wrapping -- the way George Boole's renderer does. The pips are the single
 * exception, and the only place this game reaches for GRRLIB directly, because
 * there is no shape primitive in ui_utils and a card game needs one.
 *
 * Nothing here reads the clock or the input. Animation arrives as a 0..1
 * progress value the caller has already advanced, which is what keeps the
 * renderer a pure function of what it is handed and keeps the decision about
 * when a reveal is over -- the thing input is gated on -- in one place.
 */

/* Design-space geometry, exported so the loop can place a cursor against the
   same numbers the renderer draws with rather than a second copy of them. */
#define CARD_W        76
#define CARD_H        96
#define CARD_GAP      10

#define COMMUNITY_Y   84
#define HOLE_Y       188

#define PANEL_X       40
#define PANEL_Y      320
#define PANEL_W      560
#define PANEL_H      136

/* The widest button grid the panel will lay out. Four across is what the bet
   and cash-out strips ask for; two rows of four is what banking and digging
   into savings need on the same screen. */
#define OPTION_COLS_MAX  4
#define OPTION_ROWS_MAX  2

/* Left edge of the nth community slot / hole card, 0-based. */
int render_community_x(int index);
int render_hole_x(int index);

/* One suit pip, centred on (cx, cy), `size` design units across.
 *
 * Press Start 2P has no suit glyphs, and a missing glyph on a CRT is
 * indistinguishable from a bug, so these are drawn. GRRLIB_NGoneFilled is a
 * triangle fan and therefore only honest about convex shapes: a diamond is one
 * polygon, but hearts, spades and clubs are concave and get composed from
 * circles plus a triangle. Colour carries the suit redundantly, so a pip that
 * turns to mush at distance still reads red or black. */
void render_pip(int cx, int cy, int size, int suit, u32 color);

/* A safe-area border in the game's own colour.
 *
 * magnolia's ui_draw_border() is hardcoded cyan, which is right for the two
 * games it was written for and wrong on a lava table -- it reads as a different
 * program's chrome. Same root cause as the drop shadow above: ui_utils bakes in
 * colours that a themed game needs to choose. */
void render_border(const Palette *p);

/* A colour at `alpha` of its own opacity, 0..1. Every fade in this game goes
   through here rather than through a second palette of dimmed values. */
u32 render_fade(u32 color, float alpha);

/* Centred text with no drop shadow.
 *
 * magnolia's ui_draw_text_centered_in() lays a hardcoded black shadow two
 * pixels down and right under every string. On light text over a dark panel
 * that is what gives the letters their edge -- but this game keeps putting dark
 * text on light fills, and there the shadow lands *inside* the glyphs and reads
 * as a smudge or a strike-through. It is the same limitation the card ranks hit,
 * found again on the selected menu button, which is the single most-looked-at
 * element on the screen.
 *
 * Use this wherever the fill is lighter than the ink; keep ui_utils elsewhere. */
void render_text_centered_in(int design_x, int design_y,
                             int design_w, int design_h,
                             const char *text, unsigned int design_size,
                             u32 color);

/* A dealt card: pale face, rank in the corner, a large central pip. */
void render_card(int x, int y, Card c, const Palette *p);

/* A slot that has not been dealt yet. `label` is the street it is waiting for
   -- F, T, R -- which is how the board teaches its own structure. */
void render_card_slot(int x, int y, const char *label, const Palette *p);

/* Which cards on the board are arriving this frame, for render_table().
 *
 * Cards are dealt in bursts -- two hole cards, then three, then one, then one
 * -- so "what is new" is always a contiguous run starting somewhere, and one
 * index says which. */
#define RENDER_REVEAL_NONE  (-1)   /* everything on the board is settled */
#define RENDER_REVEAL_HOLE  (-2)   /* the pre-flop deal */

/* Background, HUD, depth bar, both card rows and the hand status line.
 *
 * `shown_chips` and `shown_bank` are what the HUD should print, which is not
 * the same as what the Dome holds: those two numbers are tweened, and a
 * renderer that read them off the struct would undo the tween by drawing the
 * final value on the first frame.
 *
 * `reveal_from` is one of the RENDER_REVEAL_* constants or the index of the
 * first newly dealt community card; `reveal_t` runs 0..1 over that arrival.
 * `hand` may be an invalid result before there is anything to say. */
void render_table(const Dome *d,
                  int shown_chips, int shown_bank,
                  const Card *hole, int hole_count,
                  const Card *community, int community_count,
                  int reveal_from, float reveal_t,
                  const HandResult *hand,
                  const Palette *p);

/* The interactive area.
 *
 * Options are laid out as a grid of `cols` columns -- index i sits at row
 * i / cols, column i % cols, which is exactly how magnolia's MenuGrid moves a
 * cursor through them, so the cursor and the buttons cannot disagree about
 * where anything is. Pass 0 options for a panel that is only telling the player
 * something.
 *
 * `subtitle` and `note` may each be NULL. The note is drawn just above the
 * buttons and there is only room for it when the grid is a single row; a
 * two-row grid is already using that space. */
void render_phase_panel(const char *title,
                        const char *subtitle, u32 subtitle_color,
                        const char *const *options, int option_count, int cols,
                        int selected,
                        const char *note, u32 note_color,
                        const Palette *p);

/* The result of a resolved hand, drawn into the phase panel's space. `pulse_t`
   runs 0..1 as the panel arrives and fades its outline in; pass 1 for a panel
   that has already settled. */
void render_result_panel(const DomeResult *r, float pulse_t, const Palette *p);

#endif
