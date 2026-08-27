"""Palette, layout and card drawing — everything in character cells.

Split out so :mod:`lavadome.scenes` and :mod:`lavadome.app` can share it
without importing each other. Nothing here imports the engine beyond the
``UISurface`` protocol it draws through, and the card renderer is a plain
function so it can be unit-tested against a bare buffer.

The palette is the web build's ``LAVA_COLORS`` from ``js/config.js``.
"""

from __future__ import annotations

from typing import Any

from lavadome.cards import Card

# ── Lava palette ────────────────────────────────────────────────────
# Transcribed from web/js/config.js.

BLACK = "#0a0000"
DARK_RED = "#1c0000"
DEEP_RED = "#3b0000"
LAVA_DARK = "#6b0000"
LAVA_MID = "#9b0000"
LAVA_BRIGHT = "#cc2200"
ORANGE = "#dd4400"
ORANGE_HOT = "#ff5500"
ORANGE_GLOW = "#ff7700"
YELLOW = "#ffcc00"
YELLOW_PALE = "#ffe680"
WHITE = "#fff8f0"

BG = BLACK
PANEL_LABEL = "#8a5a4a"
PANEL_VALUE = WHITE
TITLE = ORANGE_GLOW
DIM = "#6b4038"
WIN = "#7fe07f"
LOSE = ORANGE_HOT

CARD_FACE = "#f4ece0"
CARD_RED = "#c01818"
CARD_BLACK = "#1a1a22"
CARD_BACK = "#4a0d0d"
CARD_BACK_INK = "#8a2020"
CARD_SLOT = "#1c0f0f"

MENU_BOX = "#1c0808"
MENU_SELECTED = YELLOW
MENU_SELECTION_BG = "#3b0f00"

# ── Card geometry ───────────────────────────────────────────────────
# 5x3 is the smallest that fits "10♦" with a margin and still reads as a card.

CARD_W = 5
CARD_H = 3
CARD_GAP = 1

#: The plain title, and the fallback when the block face will not fit.
BANNER = "TEXAS HOLD'EM LAVA DOME"
SUBTITLE = "solo hold'em against an escalating threshold"

#: Drawn in :mod:`texastoast.ui.bigtext` when there is room. The block face
#: carries "LAVA DOME" rather than the full name: the whole banner would be
#: 114 columns in block letters, and the part worth shouting is the dome.
BIG_TITLE = "LAVA DOME"

# ── Layout ──────────────────────────────────────────────────────────

MARGIN_X = 2
HOLE_Y = 4                                     # label sits on the row above
BOARD_Y = HOLE_Y + CARD_H + 2
INFO_Y = BOARD_Y + CARD_H + 1
STATS_Y = INFO_Y + 3
ACTIONS_Y = INFO_Y + 6

HOLE_X = MARGIN_X + 1
BOARD_X = MARGIN_X + 1

_BOARD_SPAN = 5 * CARD_W + 4 * CARD_GAP        # five community cards

MIN_COLS = max(BOARD_X + _BOARD_SPAN + 2, 58)
#: The last thing drawn is the key hints, two rows below ACTIONS_Y. Tuned so
#: the whole board fits a plain 80x24 terminal with rows to spare.
MIN_ROWS = ACTIONS_Y + 3

MENU_MIN_COLS = 46
MENU_MIN_ROWS = 16

#: The block title costs two rows more than the plain one. Below this the
#: plain banner is drawn instead, which is what keeps the screen usable at
#: MENU_MIN_ROWS.
BIG_TITLE_MIN_ROWS = MENU_MIN_ROWS + 3

MENU_W = 30
MENU_ITEM_H = 1
MENU_TITLE_H = 2
MENU_PAD = 1
MENU_BORDER = 1


def risk_color(fraction: float) -> str:
    """Colour a bet by how much of the stack it puts at risk."""
    if fraction >= 1.0:
        return YELLOW
    if fraction >= 0.75:
        return ORANGE_HOT
    if fraction >= 0.40:
        return ORANGE
    return WIN


def draw_card(surface: Any, x: int, y: int, card: Card | None, *,
              face_up: bool = True, group: str = "",
              ascii_only: bool = False) -> None:
    """Draw one 5x3 card at ``(x, y)``.

    The rank sits top-left and again bottom-right, which is what makes a block
    of colour read as a playing card rather than a swatch — the eye recognises
    the mirrored index before it reads either one.

    ``card`` of None draws an empty slot: a community position not yet dealt.
    Showing five slots from the start means the board does not jump around as
    the streets come out.
    """
    if card is None:
        surface.ui_rect(x, y, CARD_W, CARD_H, fill=CARD_SLOT, group=group)
        return

    if not face_up:
        surface.ui_rect(x, y, CARD_W, CARD_H, fill=CARD_BACK, group=group)
        for row in range(CARD_H):
            surface.ui_text(x + 1, y + row, "░░░", fill=CARD_BACK_INK, group=group)
        return

    label = card.label(ascii_only=ascii_only)
    ink = CARD_RED if card.is_red else CARD_BLACK

    surface.ui_rect(x, y, CARD_W, CARD_H, fill=CARD_FACE, group=group)
    surface.ui_text(x, y, label, fill=ink, group=group)
    # Bottom-right, right-aligned, so a two-character rank still fits.
    surface.ui_text(x + CARD_W, y + CARD_H - 1, label,
                    fill=ink, anchor="ne", group=group)


def draw_card_row(surface: Any, x: int, y: int, cards, *, slots: int = 0,
                  group: str = "", ascii_only: bool = False) -> int:
    """Draw a row of cards, padding with empty slots up to ``slots``.

    Returns the width drawn, so a caller can centre or underline it.
    """
    count = max(slots, len(cards))
    for i in range(count):
        card = cards[i] if i < len(cards) else None
        draw_card(surface, x + i * (CARD_W + CARD_GAP), y, card,
                  group=group, ascii_only=ascii_only)
    return count * CARD_W + max(0, count - 1) * CARD_GAP
