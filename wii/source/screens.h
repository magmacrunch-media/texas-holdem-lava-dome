#ifndef SCREENS_H
#define SCREENS_H

#include "magnolia.h"
#include "palette.h"

/* Everything that is not the table: the title menu and the two screens behind
   it. Kept out of main.c so the game loop stays readable -- these are long on
   text and short on logic, which is the opposite of everything around them.
   Same split, and the same reasoning, as george-boole's screens.c. */

typedef enum {
    TITLE_PLAY,
    TITLE_HOWTO,
    TITLE_SCORES,
    TITLE_CREDITS,
    TITLE_ITEM_COUNT
} TitleItem;

#define HOWTO_PAGES 3

/* The title, with the cursor wherever the menu says it is. Draws the whole
   frame including the background and border, but does not flip -- the caller
   owns renderer_finish(), because on the title an overlay may still be waiting
   to be drawn on top. */
void screens_draw_title(const Palette *p, const MenuGrid *menu);

/* The rules, three pages, left and right to turn them.
 *
 * The last page is the hand table, and it is generated from hand_points() and
 * dome_payout_hundredths() rather than typed out. A copied table is a second
 * home for the balance numbers, and the one that nobody re-checks when the
 * first one moves -- the whole README argues against exactly that. */
void screens_draw_howto(const Palette *p, int page);

void screens_draw_credits(const Palette *p);

const char *screens_title_label(int item);

#endif
