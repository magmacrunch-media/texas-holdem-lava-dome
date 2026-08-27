"""The screens, as scenes.

Modality is the stack, not a flag — the engine's rule. `TitleScene` sits at the
bottom; starting a run pushes a `GameScene` over it and Esc pops back, and the
rules screen pushes over whichever is showing. Nothing holds an `on_title`
boolean, and a scene that has been popped stops getting frames because the
stack simply does not call it.

Everything draws through texastoast's ``Renderer``/``UISurface`` protocols and
none of it knows what Textual is.
"""

from __future__ import annotations

import textwrap
from dataclasses import replace

from texastoast.ui import DEFAULT_THEME, Menu

from lavadome import config, theme
from lavadome.betting import Betting
from lavadome.dealer import Dealer
from lavadome.dome import Dome
from lavadome.handeval import HandEvaluator
from lavadome.state import GameState

#: Paragraphs, not lines. They are wrapped to the terminal at render time —
#: hardcoding the breaks makes the screen correct at exactly one width and
#: clipped at every narrower one.
RULES = (
    "You are alone in the dome. There is no opponent, only a threshold.",
    "Each round the dome charges an ante, deals you two cards and five to "
    "the board, and scores your best five-card hand in points. Beat the "
    "round's threshold and your stake comes back with a multiplier; miss it "
    "and the stake is gone.",
    "Both the ante and the threshold climb every round. A high card scores "
    "nothing, so it can never clear the dome.",
    "After a hand you choose: bank chips, or leave them in play. Banked chips "
    "cannot be lost — the bank is your score. Chips still in play are what "
    "the next ante comes out of.",
    "The run ends when the ante takes your last chip, or when you escape with "
    "what you have banked.",
)


def _menu_theme():
    """The engine's Theme in the lava palette. Frozen, so ``replace``."""
    return replace(
        DEFAULT_THEME,
        primary=theme.MENU_SELECTED,
        text=theme.PANEL_VALUE,
        dim_text=theme.DIM,
        box_fill=theme.MENU_BOX,
        box_outline=theme.LAVA_DARK,
        outline_width=1,
        selection_fill=theme.MENU_SELECTION_BG,
    )


def _too_small(renderer, cols: int, rows: int) -> bool:
    if renderer.width >= cols and renderer.height >= rows:
        return False
    renderer.ui_text(
        1, 1,
        f"terminal too small — need {cols}x{rows}, "
        f"have {renderer.width}x{renderer.height}",
        fill=theme.LOSE,
    )
    return True


class TitleScene:
    """Landing screen."""

    ITEMS = ("DESCEND INTO THE DOME", "HOW TO PLAY", "QUIT")

    def __init__(self, app):
        self.app = app
        self.menu = Menu(
            app.renderer,
            theme=_menu_theme(),
            # Cells, not pixels — the engine's defaults are 280 wide with
            # 32-cell rows, which is entirely off-screen in a terminal.
            menu_width=theme.MENU_W,
            item_height=theme.MENU_ITEM_H,
            title_height=theme.MENU_TITLE_H,
            item_padding=theme.MENU_PAD,
            border_pad=theme.MENU_BORDER,
            selected_color=theme.MENU_SELECTED,
            normal_color=theme.PANEL_VALUE,
        )
        self._show()

    def _show(self) -> None:
        self.menu.show(list(self.ITEMS), on_select=self._chose)

    def _chose(self, index: int, label: str) -> None:  # noqa: ARG002
        if index == 0:
            self.app.start_run()
        elif index == 1:
            self.app.show_rules()
        else:
            self.app.game.quit()

    def on_resume(self) -> None:
        # Menu.confirm() hides the menu as it fires, so the screen underneath
        # would otherwise come back blank.
        self._show()

    def handle_key(self, key: str) -> bool:
        if key in ("up", "w", "k"):
            self.menu.move_up()
        elif key in ("down", "s", "j"):
            self.menu.move_down()
        elif key in ("enter", "space"):
            self.menu.confirm()
        elif key == "q":
            self.app.game.quit()
        elif key == "h":
            self.app.show_rules()
        else:
            return False
        return True

    def update(self, dt: float) -> None:
        pass

    def render(self) -> None:
        r = self.app.renderer
        r.clear()
        r.draw_rect(0, 0, r.width, r.height, theme.BG)
        if _too_small(r, theme.MENU_MIN_COLS, theme.MENU_MIN_ROWS):
            r.present()
            return

        cx = r.width // 2
        r.ui_text(cx, 1, theme.BANNER, fill=theme.TITLE, anchor="n")
        r.ui_text(cx, 2, "solo hold'em against an escalating threshold",
                  fill=theme.DIM, anchor="n")
        self.menu.render()

        if self.app.best_bank:
            r.ui_text(cx, r.height - 3, f"best bank: {self.app.best_bank}",
                      fill=theme.PANEL_LABEL, anchor="n")
        r.ui_text(cx, r.height - 2, "↑↓ choose    Enter select    Q quit",
                  fill=theme.DIM, anchor="n")
        r.present()


class RulesScene:
    """How to play. Pushed over whatever is showing."""

    def __init__(self, app):
        self.app = app

    def handle_key(self, key: str) -> bool:  # noqa: ARG002
        self.app.pop_scene()
        return True

    def update(self, dt: float) -> None:
        pass

    #: Widest payout row is "Three of a Kind  1.5x" plus a gutter.
    PAYOUT_COL_W = 22

    def render(self) -> None:
        r = self.app.renderer
        r.clear()
        r.draw_rect(0, 0, r.width, r.height, theme.BG)
        if _too_small(r, theme.MENU_MIN_COLS, theme.MENU_MIN_ROWS):
            r.present()
            return

        avail = max(20, r.width - theme.MARGIN_X * 2)
        r.ui_text(theme.MARGIN_X, 1, "HOW TO PLAY", fill=theme.TITLE)

        # One row is reserved at the bottom for the dismissal hint, and the
        # payout table is only drawn if what is left can hold all of it —
        # half a table is worse than none.
        names = [n for n in config.PAYOUT_MULTIPLIERS if n != "High Card"]
        cols = max(1, avail // self.PAYOUT_COL_W)
        rows = (len(names) + cols - 1) // cols
        budget = r.height - 2 - (rows + 2)

        y = 3
        for paragraph in RULES:
            if y >= budget:
                break
            for line in textwrap.wrap(paragraph, avail):
                if y >= budget:
                    break
                r.ui_text(theme.MARGIN_X, y, line, fill=theme.PANEL_VALUE)
                y += 1
            y += 1

        if y + rows + 1 < r.height - 1:
            r.ui_text(theme.MARGIN_X, y, "PAYOUTS", fill=theme.PANEL_LABEL)
            for i, name in enumerate(names):
                col, row = divmod(i, rows)
                mult = config.PAYOUT_MULTIPLIERS[name]
                r.ui_text(theme.MARGIN_X + col * self.PAYOUT_COL_W, y + 1 + row,
                          f"{name:<16}{mult:g}x", fill=theme.YELLOW_PALE)

        r.ui_text(theme.MARGIN_X, r.height - 1, "any key to go back",
                  fill=theme.DIM)
        r.present()


class GameScene:
    """A run: rounds until the dome takes the last chip, or you escape."""

    def __init__(self, app):
        self.app = app
        self.state = GameState()
        self.dealer = Dealer(self.state, app.rng)
        self.dome = Dome(self.state, HandEvaluator(), app.rng)
        self.betting = Betting(self.state)

        self.pending_bet = 0
        self.message = ""
        self.resolution: dict | None = None
        self.ending: dict | None = None
        #: The hand as it stands, refreshed when a card lands. Not computed in
        #: render(): seven cards is 21 five-card combinations and the render
        #: pass runs 20 times a second.
        self.preview = None

        self.state.reset()
        self.state.round = 1
        self._begin_round()

    # ── Flow ────────────────────────────────────────────────────────

    def _begin_round(self) -> None:
        self.resolution = None
        self.dealer.new_round()
        charged = self.dome.charge_ante()
        if charged.get("bust"):
            self._end(charged)
            return
        self.dealer.deal_hole_cards()
        self.pending_bet = self.betting.min_bet
        self.message = ""
        self._refresh_preview()

    def _end(self, result: dict) -> None:
        self.ending = result
        self.app.record_bank(self.state.bank)

    @property
    def over(self) -> bool:
        return self.ending is not None

    def _resolve(self) -> None:
        """Grade the hand on the way into the phase, never during a render.

        The web build calls resolveHand() from its render path and gets away
        with it because resolving moves the phase on. A terminal redraws on
        every resize, so awarding chips from a render would be a live bug here.
        """
        self.resolution = self.dome.resolve_hand()
        self.app.record_bank(self.state.bank + self.state.chips)

    def _advance(self) -> None:
        self.dealer.advance_street()
        self._refresh_preview()
        if self.state.phase == "resolve":
            self._resolve()

    def _refresh_preview(self) -> None:
        cards = self.state.all_cards
        self.preview = (self.dome.evaluator.evaluate(cards)
                        if len(cards) >= 2 else None)

    def _next_round(self) -> None:
        result = self.dome.start_next_round()
        if result.get("bust"):
            self._end(result)
            return
        self._begin_round()

    # ── Input ───────────────────────────────────────────────────────

    def handle_key(self, key: str) -> bool:
        if key == "q":
            self.app.game.quit()
            return True
        if key == "h":
            self.app.show_rules()
            return True
        if key == "escape":
            self.app.pop_scene()
            return True
        if self.over:
            if key in ("enter", "space", "n"):
                self.app.pop_scene()
            return True

        phase = self.state.phase
        if phase == "betting":
            return self._keys_betting(key)
        if phase in ("flop", "turn", "river"):
            return self._keys_street(key)
        if phase == "cashout":
            return self._keys_cashout(key)
        return False

    def _keys_betting(self, key: str) -> bool:
        step = config.MIN_BET
        if key in ("left", "a", "-"):
            self.pending_bet = max(self.betting.min_bet, self.pending_bet - step)
        elif key in ("right", "d", "+", "="):
            self.pending_bet = min(self.state.chips, self.pending_bet + step)
        elif key in ("up", "w"):
            self.pending_bet = self.state.chips
        elif key in ("down", "s"):
            self.pending_bet = self.betting.min_bet
        elif len(key) == 1 and key in "1234":
            picks = self.betting.suggested_bets
            idx = int(key) - 1
            if idx < len(picks):
                self.pending_bet = picks[idx]
        elif key in ("b", "enter"):
            result = self.betting.place_bet(self.pending_bet)
            if not result["ok"]:
                self.message = result["error"]
                return True
            self.message = ""
            self._advance()
        elif key == "c":
            self.betting.check()
            self._advance()
        else:
            return False
        return True

    def _keys_street(self, key: str) -> bool:
        step = config.MIN_BET
        if key in ("left", "a", "-"):
            self.pending_bet = max(self.betting.min_bet, self.pending_bet - step)
        elif key in ("right", "d", "+", "="):
            self.pending_bet = min(self.state.chips, self.pending_bet + step)
        elif key in ("space", "enter"):
            self._advance()
        elif key == "r":
            result = self.betting.raise_bet(self.pending_bet)
            if not result["ok"]:
                self.message = result["error"]
                return True
            self.message = ""
            # A raise buys the next card: one decision per street and never an
            # open betting loop. The web build and the Wii port both do this.
            self._advance()
        elif key == "f":
            self.betting.fold()
            self.dealer.fold()
            self.resolution = None
            self.message = "Folded."
        else:
            return False
        return True

    def _keys_cashout(self, key: str) -> bool:
        if key == "k":
            self.betting.cash_out_all()
        elif len(key) == 1 and key in "1234":
            picks = self.betting.suggested_cash_outs
            idx = int(key) - 1
            if idx < len(picks):
                self.betting.cash_out_partial(picks[idx])
        elif key == "w":
            picks = self.betting.suggested_withdrawals
            if picks:
                self.betting.withdraw_from_bank(picks[0])
        elif key == "e":
            if self.state.can_escape or self.state.chips == 0:
                self._end(self.dome.escape())
        elif key in ("n", "space", "enter"):
            self._next_round()
        else:
            return False
        self.app.record_bank(self.state.bank + self.state.chips)
        return True

    def update(self, dt: float) -> None:
        pass

    # ── Render ──────────────────────────────────────────────────────

    def render(self) -> None:
        r = self.app.renderer
        r.clear()
        r.draw_rect(0, 0, r.width, r.height, theme.BG)
        if _too_small(r, theme.MIN_COLS, theme.MIN_ROWS):
            r.present()
            return

        if self.over:
            self._render_ending()
            r.present()
            return

        self._render_header()
        self._render_cards()
        self._render_hand_line()
        self._render_stats()
        self._render_actions()
        r.present()

    def _render_header(self) -> None:
        r = self.app.renderer
        r.ui_text(theme.MARGIN_X, 0, theme.BANNER, fill=theme.TITLE)
        r.ui_text(r.width - theme.MARGIN_X, 0, f"ROUND {self.state.round}",
                  fill=theme.PANEL_VALUE, anchor="ne")
        r.ui_text(theme.MARGIN_X, 1, self.state.current_depth_label,
                  fill=theme.DIM)

    def _render_cards(self) -> None:
        r = self.app.renderer
        ascii_only = self.app.ascii_only

        r.ui_text(theme.MARGIN_X, theme.HOLE_Y - 1, "YOUR HAND",
                  fill=theme.PANEL_LABEL)
        theme.draw_card_row(r, theme.HOLE_X, theme.HOLE_Y,
                            self.state.hole_cards, slots=2,
                            ascii_only=ascii_only)

        label = self.dealer.community_label or "Pre-Flop"
        r.ui_text(theme.MARGIN_X, theme.BOARD_Y - 1, f"THE BOARD — {label}",
                  fill=theme.PANEL_LABEL)
        theme.draw_card_row(r, theme.BOARD_X, theme.BOARD_Y,
                            self.state.community_cards, slots=5,
                            ascii_only=ascii_only)

    def _render_hand_line(self) -> None:
        r = self.app.renderer
        y = theme.INFO_Y
        cards = self.state.all_cards
        if len(cards) < 2:
            return

        # Read only — the preview is refreshed when a card lands, never here.
        result = self.state.best_hand if self.resolution else self.preview
        if result is None:
            return
        name = result.description or result.name
        points = result.points
        threshold = self.state.dome_threshold

        clears = points >= threshold
        r.ui_text(theme.MARGIN_X, y, name.upper(), fill=theme.PANEL_VALUE)
        r.ui_text(theme.MARGIN_X, y + 1,
                  f"{points} PTS   vs   DOME {threshold} PTS",
                  fill=theme.WIN if clears else theme.LOSE)

    def _render_stats(self) -> None:
        r = self.app.renderer
        y = theme.STATS_Y
        parts = [
            ("CHIPS", f"{self.state.chips}", theme.PANEL_VALUE),
            ("BANK", f"{self.state.bank}", theme.YELLOW),
            ("BET", f"{self.state.current_bet}", theme.ORANGE_GLOW),
            ("ANTE", f"{self.state.ante}", theme.PANEL_LABEL),
        ]
        x = theme.MARGIN_X
        for label, value, color in parts:
            r.ui_text(x, y, label, fill=theme.PANEL_LABEL)
            r.ui_text(x, y + 1, value, fill=color)
            x += max(len(label), len(value)) + 4

    def _render_actions(self) -> None:
        r = self.app.renderer
        y = theme.ACTIONS_Y
        phase = self.state.phase

        if self.resolution:
            self._render_resolution(y)
            return

        if phase == "betting":
            fraction = self.betting.risk_fraction(self.pending_bet)
            r.ui_text(theme.MARGIN_X, y, f"BET  {self.pending_bet}",
                      fill=theme.risk_color(fraction))
            picks = "  ".join(f"[{i+1}] {v}"
                              for i, v in enumerate(self.betting.suggested_bets))
            r.ui_text(theme.MARGIN_X + 16, y, picks, fill=theme.DIM)
            keys = "←→ adjust   B bet   C check for free   H help   Esc leave"
        elif phase in ("flop", "turn", "river"):
            r.ui_text(theme.MARGIN_X, y, f"RAISE  {self.pending_bet}",
                      fill=theme.risk_color(
                          self.betting.risk_fraction(self.pending_bet)))
            keys = "Space next card   R raise   F fold   H help   Esc leave"
        elif phase == "cashout":
            keys = "K bank all   1-4 bank some   W withdraw   N next round   E escape"
        else:
            keys = "Space continue"

        if self.message:
            r.ui_text(theme.MARGIN_X, y + 1, self.message, fill=theme.LOSE)
        r.ui_text(theme.MARGIN_X, y + 2, keys, fill=theme.DIM)

    def _render_resolution(self, y: int) -> None:
        r = self.app.renderer
        res = self.resolution
        assert res is not None
        won = res["beat_dome"]

        r.ui_text(theme.MARGIN_X, y,
                  "▲ BEAT THE DOME" if won else "▼ THE DOME WINS",
                  fill=theme.WIN if won else theme.LOSE)
        r.ui_text(theme.MARGIN_X + 18, y,
                  f"{'+' if won else '-'}{res['chips_won'] if won else res['chips_lost']}"
                  f" CHIPS",
                  fill=theme.WIN if won else theme.LOSE)
        r.ui_text(theme.MARGIN_X, y + 1, res["flavor"], fill=theme.YELLOW_PALE)
        r.ui_text(theme.MARGIN_X, y + 2,
                  "K bank all   1-4 bank some   N next round   E escape",
                  fill=theme.DIM)

    def _render_ending(self) -> None:
        r = self.app.renderer
        end = self.ending
        assert end is not None
        escaped = end.get("escaped")
        cx = r.width // 2

        r.ui_text(cx, 3, "YOU ESCAPED THE DOME" if escaped else "THE DOME KEPT YOU",
                  fill=theme.WIN if escaped else theme.LOSE, anchor="n")
        r.ui_text(cx, 5, end.get("flavor", ""), fill=theme.YELLOW_PALE, anchor="n")
        r.ui_text(cx, 7, f"BANKED   {end.get('final_bank', 0)}",
                  fill=theme.YELLOW, anchor="n")
        r.ui_text(cx, 8, f"ROUNDS   {end.get('rounds', 0)}",
                  fill=theme.PANEL_VALUE, anchor="n")
        r.ui_text(cx, 9, end.get("depth_label", ""), fill=theme.DIM, anchor="n")
        if end.get("reason"):
            r.ui_text(cx, 11, end["reason"], fill=theme.PANEL_LABEL, anchor="n")
        r.ui_text(cx, r.height - 2, "Enter for the title screen    Q quit",
                  fill=theme.DIM, anchor="n")
