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

/* A dealt card: pale face, rank in the corner, a large central pip. */
void render_card(int x, int y, Card c, const Palette *p);

/* A slot that has not been dealt yet. `label` is the street it is waiting for
   -- F, T, R -- which is how the board teaches its own structure. */
void render_card_slot(int x, int y, const char *label, const Palette *p);

/* Background, HUD, depth bar, both card rows and the hand status line.
   `hand` may be an invalid result before there is anything to say. */
void render_table(const Dome *d,
                  const Card *hole, int hole_count,
                  const Card *community, int community_count,
                  const HandResult *hand,
                  const Palette *p);

/* The interactive area. Options are drawn as a horizontal row of buttons with
   `selected` highlighted; pass 0 options for a panel that is only telling the
   player something. */
void render_phase_panel(const char *title, const char *subtitle,
                        const char *const *options, int option_count,
                        int selected, const Palette *p);

/* The result of a resolved hand, drawn into the phase panel's space. */
void render_result_panel(const DomeResult *r, const Palette *p);

#endif
