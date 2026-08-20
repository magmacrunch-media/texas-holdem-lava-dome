#include "palette.h"
#include "cards.h"

/* Every value here is one of the twelve in LAVA_COLORS, unchanged. Nothing is
   mixed or nudged: the web build and the console build should be recognisably
   the same game, and "close enough by eye on a monitor" is how two versions
   drift apart. The comment on each line is the name it has in the stylesheet. */
static const Palette LAVA = {
    .table_bg     = PAL_RGB(0x0a0000),   /* black */
    .panel_bg     = PAL_RGB(0x1c0000),   /* darkRed */
    .panel_border = PAL_RGB(0x6b0000),   /* lavaDark */

    .card_face    = PAL_RGB(0xfff8f0),   /* white */
    .card_border  = PAL_RGB(0x3b0000),   /* deepRed */
    .card_back    = PAL_RGB(0x9b0000),   /* lavaMid */
    .slot_bg      = PAL_RGB(0x1c0000),   /* darkRed */
    .slot_border  = PAL_RGB(0x3b0000),   /* deepRed */

    .pip_red      = PAL_RGB(0xcc2200),   /* lavaBright */
    .pip_black    = PAL_RGB(0x0a0000),   /* black */

    .text         = PAL_RGB(0xfff8f0),   /* white */
    .text_dim     = PAL_RGB(0xdd4400),   /* orange */
    .accent       = PAL_RGB(0xff7700),   /* orangeGlow */
    .highlight    = PAL_RGB(0xffcc00),   /* yellow */

    .win          = PAL_RGB(0xffcc00),   /* yellow */
    .loss         = PAL_RGB(0xcc2200)    /* lavaBright */
};

const Palette *palette_lava(void) {
    return &LAVA;
}

u32 palette_pip_color(const Palette *p, int suit) {
    return card_suit_is_red(suit) ? p->pip_red : p->pip_black;
}
