#ifndef PALETTE_H
#define PALETTE_H

/* Colours are just packed RGBA, so this header stays usable on a development
   machine where <gccore.h> does not exist -- otherwise the palette would be the
   one part of the game that could only be checked by looking at a television.
   devkitPPC defines GEKKO for the console build. */
#ifdef GEKKO
#include <gccore.h>
#else
typedef unsigned int u32;
#endif

/* Packed the way GRRLIB reads a colour (r<<24 | g<<16 | b<<8 | a), spelled out
   rather than borrowed from RGBA(): that macro is not a constant expression in
   the console build, so a static palette cannot be initialised with it. */
#define PAL_RGBA(r, g, b, a)                                     \
    ((u32)(((u32)((r) & 0xFF) << 24) | ((u32)((g) & 0xFF) << 16)  \
         | ((u32)((b) & 0xFF) << 8)  |  (u32)((a) & 0xFF)))

/* From a 0xRRGGBB literal, which is how the values read in the stylesheet. */
#define PAL_RGB(hex)                        \
    PAL_RGBA(((hex) >> 16) & 0xFF,          \
             ((hex) >> 8)  & 0xFF,          \
              (hex)        & 0xFF, 255)

/* The lava palette, transcribed from LAVA_COLORS in the web build's
 * js/config.js. One palette, not a table: this game has a single visual
 * identity, unlike George Boole where each bit mode wears a console era.
 *
 * Fields are named for the job they do rather than the colour they are, so a
 * retune moves values without renaming everything that reads them.
 *
 * Two notes on the choices. Card faces are the palette's near-white (#fff8f0)
 * because a playing card that is not pale stops reading as a playing card, and
 * the warmth keeps it from looking pasted on. And there is no green anywhere in
 * the source palette, so a win is gold rather than green -- which suits a game
 * about chips better anyway.
 */
typedef struct {
    u32 table_bg;       /* behind everything */
    u32 panel_bg;       /* HUD and phase panels */
    u32 panel_border;

    u32 card_face;      /* a dealt card */
    u32 card_border;
    u32 card_back;      /* face down */
    u32 slot_bg;        /* a community slot not yet dealt */
    u32 slot_border;

    u32 pip_red;        /* hearts and diamonds */
    u32 pip_black;      /* spades and clubs */

    u32 text;
    u32 text_dim;
    u32 accent;         /* the hot orange: the dome, and anything urgent */
    u32 highlight;      /* the selected item in a list */

    u32 win;
    u32 loss;

    /* The bet-risk badge. Only two fields because the other two levels already
       have a colour that means the same thing: medium risk is the accent the
       dome itself is drawn in, and all-in is simply `loss` said early.

       Both values are LAVA_COLORS entries that nothing else had claimed yet, so
       the badge introduces no colour the web build does not have. */
    u32 risk_low;
    u32 risk_high;
} Palette;

/* The one palette. Returned by pointer to match how the renderer threads it
   around, and so a second look (a darker deep-round variant, say) can be added
   later without changing every call site. */
const Palette *palette_lava(void);

/* Pip colour for a suit, so no draw site has to remember which suits are red. */
u32 palette_pip_color(const Palette *p, int suit);

#endif
