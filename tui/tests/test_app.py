"""The screens, driven headlessly.

What `test_rules.py` and `test_handeval.py` cannot cover: that the rules are
wired to the screen, and that the title, rules and game screens hand off to
each other correctly. Needs the engine and its terminal extra; skipped without
them so the rule tests still run on a bare checkout.
"""

import asyncio

import pytest

pytest.importorskip("textual", reason='needs: pip install -e ".[dev]" with texastoast[tui]')

from lavadome import config, theme  # noqa: E402
from lavadome.app import LavaDomeApp  # noqa: E402
from lavadome.cards import Card  # noqa: E402
from lavadome.scenes import GameScene, RulesScene, TitleScene  # noqa: E402


def buffer_text(app: LavaDomeApp) -> str:
    return app.game.surface.buffer.to_text()


def settle(app: LavaDomeApp) -> None:
    """Apply the stack's pending push/pop, which the engine defers a frame."""
    app.stack.update(0.0)


def game_app(seed=2024) -> tuple[LavaDomeApp, GameScene]:
    app = LavaDomeApp(seed=seed, skip_title=True)
    settle(app)
    return app, app.stack.top


def hand(*specs: str) -> list[Card]:
    suits = {"s": "spades", "h": "hearts", "d": "diamonds", "c": "clubs"}
    return [Card(suits[spec[-1]], spec[:-1]) for spec in specs]


async def _piloted(app: LavaDomeApp, size=(80, 24)):
    from texastoast.core.tui_game import _GameApp

    textual_app = _GameApp(app.game, app.game.surface)
    app.game._app = textual_app
    return textual_app.run_test(size=size)


def run(coro):
    return asyncio.run(coro)


# ── The stack ───────────────────────────────────────────────────────


def test_the_title_screen_is_the_bottom_of_the_stack():
    app = LavaDomeApp(seed=1)
    settle(app)
    assert isinstance(app.stack.top, TitleScene)
    assert not app.in_game


def test_descending_pushes_a_run_over_the_title():
    app = LavaDomeApp(seed=1)
    settle(app)
    app.stack.top.handle_key("enter")
    settle(app)
    assert app.in_game
    assert len(app.stack) == 2
    assert isinstance(app.stack.scenes[0], TitleScene)


def test_escape_returns_to_the_title():
    app, scene = game_app()
    scene.handle_key("escape")
    settle(app)
    assert isinstance(app.stack.top, TitleScene)


def test_the_title_menu_works_again_after_a_run_is_popped():
    app = LavaDomeApp(seed=1)
    settle(app)
    app.stack.top.handle_key("enter")
    settle(app)
    app.stack.top.handle_key("escape")
    settle(app)
    assert app.stack.top.menu.active


def test_the_rules_screen_pushes_over_a_run_and_any_key_dismisses_it():
    app, scene = game_app()
    scene.handle_key("h")
    settle(app)
    assert isinstance(app.stack.top, RulesScene)

    app.stack.top.handle_key("x")
    settle(app)
    assert app.in_game


def test_the_key_that_dismisses_the_rules_does_not_also_act_on_the_run():
    # dispatch_key reaches the top scene only, so the keystroke that closes the
    # rules is consumed there. Otherwise opening the help mid-hand would cost
    # you a bet on the way out of it.
    app, scene = game_app()
    scene.handle_key("h")
    settle(app)
    chips = scene.state.chips

    app.stack.dispatch_key("b")         # would place a bet if it reached
    settle(app)
    assert scene.state.chips == chips
    assert app.in_game                  # the rules screen is gone

    app.stack.dispatch_key("b")         # now it reaches
    assert scene.state.chips < chips


def test_a_run_stops_receiving_frames_while_the_rules_are_on_top():
    app, scene = game_app()
    scene.handle_key("h")
    settle(app)
    assert app.stack.top is not scene
    # Modality is the stack: RulesScene sets no update_below, so the run
    # underneath is not in the update slice at all.
    assert scene not in app.stack._slice("update_below")


# ── Round flow ──────────────────────────────────────────────────────


def test_a_run_opens_mid_hand_with_the_ante_paid():
    _, scene = game_app()
    assert scene.state.round == 1
    assert len(scene.state.hole_cards) == 2
    assert scene.state.phase == "betting"
    assert scene.state.chips == config.STARTING_CHIPS - config.ante_for_round(1)


def test_betting_then_advancing_walks_the_streets():
    _, scene = game_app()
    scene.handle_key("b")
    assert scene.state.phase == "flop"
    assert len(scene.state.community_cards) == 3

    scene.handle_key("space")
    assert scene.state.phase == "turn"
    scene.handle_key("space")
    assert scene.state.phase == "river"
    scene.handle_key("space")
    # The river advance resolves, which moves straight on to cashout.
    assert scene.state.phase == "cashout"
    assert scene.resolution is not None


def test_checking_takes_the_flop_for_free():
    _, scene = game_app()
    chips = scene.state.chips
    scene.handle_key("c")
    assert scene.state.phase == "flop"
    assert scene.state.chips == chips
    assert scene.state.current_bet == 0


def test_a_raise_buys_the_next_card():
    # One decision per street and never an open betting loop — the web build
    # and the Wii port both do this.
    _, scene = game_app()
    scene.handle_key("b")
    assert scene.state.phase == "flop"
    before = scene.state.current_bet
    scene.handle_key("r")
    assert scene.state.current_bet > before
    assert scene.state.phase == "turn"


def test_folding_ends_the_hand_without_resolving():
    _, scene = game_app()
    scene.handle_key("b")
    scene.handle_key("f")
    assert scene.state.phase == "cashout"
    assert scene.resolution is None
    assert not scene.state.beat_dome


def test_a_rejected_bet_shows_a_message_and_changes_nothing():
    _, scene = game_app()
    scene.pending_bet = scene.state.chips + 1000
    chips = scene.state.chips
    scene.handle_key("b")
    assert scene.message
    assert scene.state.chips == chips
    assert scene.state.phase == "betting"


def test_the_bet_can_be_adjusted_within_the_stack():
    _, scene = game_app()
    scene.pending_bet = scene.betting.min_bet
    scene.handle_key("left")
    assert scene.pending_bet == scene.betting.min_bet     # clamped
    scene.handle_key("right")
    assert scene.pending_bet == scene.betting.min_bet + config.MIN_BET
    scene.handle_key("up")
    assert scene.pending_bet == scene.state.chips         # all in
    scene.handle_key("right")
    assert scene.pending_bet == scene.state.chips         # clamped


def test_quick_picks_select_a_suggested_bet():
    _, scene = game_app()
    picks = scene.betting.suggested_bets
    scene.handle_key("2")
    assert scene.pending_bet == picks[1]


def test_banking_moves_chips_out_of_reach_and_the_next_round_starts():
    _, scene = game_app()
    scene.handle_key("b")
    for _ in range(3):
        scene.handle_key("space")
    assert scene.state.phase == "cashout"

    scene.handle_key("k")               # bank all
    assert scene.state.chips == 0
    banked = scene.state.bank

    scene.handle_key("n")               # next round
    assert scene.state.bank == banked
    # No chips left in play, so the ante could not be paid: that is a bust.
    assert scene.over or scene.state.round == 2


def test_escaping_ends_the_run_with_the_bank_intact():
    _, scene = game_app()
    scene.handle_key("b")
    for _ in range(3):
        scene.handle_key("space")
    scene.state.bank = 120
    scene.handle_key("e")
    assert scene.over
    assert scene.ending["escaped"]
    assert scene.ending["final_bank"] >= 120


def test_a_run_ends_when_the_ante_cannot_be_paid():
    _, scene = game_app()
    scene.state.chips = 1
    scene.state.bank = 0
    scene.state.phase = "cashout"
    scene.handle_key("n")
    assert scene.over
    assert scene.ending.get("bust")


def test_the_hand_preview_refreshes_only_when_a_card_lands():
    _, scene = game_app()
    first = scene.preview
    assert first is not None
    scene.render()
    scene.render()
    assert scene.preview is first       # a render must not recompute it
    scene.handle_key("c")
    assert scene.preview is not first


def test_the_best_bank_survives_a_run_ending():
    app, scene = game_app()
    scene.state.bank = 420
    scene.handle_key("b")
    for _ in range(3):
        scene.handle_key("space")
    scene.handle_key("e")
    assert app.best_bank >= 420


def test_a_seeded_run_is_reproducible():
    def play(seed):
        _, scene = game_app(seed=seed)
        scene.handle_key("b")
        for _ in range(3):
            scene.handle_key("space")
        return ([(c.suit, c.rank) for c in scene.state.all_cards],
                scene.state.chips, scene.state.bank)

    assert play(77) == play(77)


# ── Rendering ───────────────────────────────────────────────────────


def test_the_title_screen_renders():
    async def go():
        app = LavaDomeApp(seed=1)
        settle(app)
        async with await _piloted(app) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            text = buffer_text(app)
            assert theme.BANNER in text
            assert "DESCEND INTO THE DOME" in text
            assert "HOW TO PLAY" in text
            app.game.quit()

    run(go())


def test_the_table_renders_cards_stats_and_actions():
    async def go():
        app, scene = game_app()
        async with await _piloted(app) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            text = buffer_text(app)
            assert "YOUR HAND" in text
            assert "THE BOARD" in text
            assert "CHIPS" in text
            assert "BANK" in text
            assert "DOME" in text
            app.game.quit()

    run(go())


def test_glyphs_keep_the_background_of_the_card_they_sit_on():
    # The compositing bug George Boole caught: a glyph must not punch a hole
    # through the card face it is printed on.
    async def go():
        app, _ = game_app()
        async with await _piloted(app) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            buf = app.game.surface.buffer
            holes = [(x, y) for y in range(buf.height) for x in range(buf.width)
                     if buf.get(x, y).char != " " and buf.get(x, y).bg is None]
            assert holes == []
            app.game.quit()

    run(go())


def test_a_whole_hand_plays_through_real_keypresses():
    async def go():
        app, scene = game_app()
        async with await _piloted(app) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            for key in ["b", "space", "space", "space"]:
                await pilot.press(key)
                await asyncio.sleep(0.1)
            assert scene.state.phase == "cashout"
            assert scene.resolution is not None
            text = buffer_text(app)
            assert ("BEAT THE DOME" in text) or ("THE DOME WINS" in text)
            app.game.quit()

    run(go())


def test_the_ending_screen_renders():
    async def go():
        app, scene = game_app()
        async with await _piloted(app) as pilot:
            await pilot.pause()
            scene.ending = {"escaped": True, "final_bank": 1234, "rounds": 6,
                            "flavor": "Hello World, Love Space.",
                            "depth_label": "Pendant Stop"}
            scene.render()
            text = buffer_text(app)
            assert "YOU ESCAPED THE DOME" in text
            assert "1234" in text
            app.game.quit()

    run(go())


def test_a_too_small_terminal_says_so_rather_than_clipping():
    async def go():
        app, _ = game_app()
        async with await _piloted(app, size=(theme.MIN_COLS, theme.MIN_ROWS)) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            assert "too small" not in buffer_text(app)

            await pilot.resize_terminal(theme.MIN_COLS - 1, theme.MIN_ROWS)
            await asyncio.sleep(0.25)
            assert "too small" in buffer_text(app)
            app.game.quit()

    run(go())


def test_the_layout_fits_a_plain_eighty_by_twenty_four_terminal():
    assert theme.MIN_COLS <= 80
    assert theme.MIN_ROWS <= 24


def test_resizing_does_not_corrupt_the_layout():
    async def go():
        app, _ = game_app()
        async with await _piloted(app, size=(80, 24)) as pilot:
            await pilot.pause()
            for width, height in [(120, 40), (58, 22), (90, 30)]:
                await pilot.resize_terminal(width, height)
                await asyncio.sleep(0.2)
                assert app.renderer.width == width
                for line in buffer_text(app).split("\n"):
                    assert len(line) <= width
            app.game.quit()

    run(go())


# ── Card drawing ────────────────────────────────────────────────────


def test_a_card_shows_its_rank_twice_mirrored():
    from texastoast.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("spades", "A"))
    lines = r.to_text().split("\n")
    assert lines[0].startswith("A♠")
    assert lines[2].rstrip().endswith("A♠")


def test_a_ten_still_fits_the_card():
    from texastoast.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("diamonds", "10"))
    assert "10♦" in r.to_text()


def test_ascii_mode_swaps_the_suit_glyphs_for_letters():
    from texastoast.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("hearts", "K"), ascii_only=True)
    text = r.to_text()
    assert "KH" in text
    assert "♥" not in text


def test_an_undealt_slot_draws_nothing_but_a_background():
    from texastoast.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, None)
    assert r.to_text().strip() == ""
    assert r.buffer.get(0, 0).bg == theme.CARD_SLOT


def test_the_board_always_shows_five_positions():
    from texastoast.render.tui import TuiRenderer

    r = TuiRenderer(40, 4)
    width = theme.draw_card_row(r, 0, 0, hand("As", "Kh"), slots=5)
    assert width == 5 * theme.CARD_W + 4 * theme.CARD_GAP
    # The two dealt cards are face up, the other three are empty slots.
    assert r.buffer.get(0, 0).bg == theme.CARD_FACE
    assert r.buffer.get(4 * (theme.CARD_W + theme.CARD_GAP), 0).bg == theme.CARD_SLOT


def test_red_and_black_suits_are_inked_differently():
    from texastoast.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("hearts", "9"))
    assert r.buffer.get(0, 0).fg == theme.CARD_RED
    r.clear()
    theme.draw_card(r, 0, 0, Card("clubs", "9"))
    assert r.buffer.get(0, 0).fg == theme.CARD_BLACK


def test_risk_colour_climbs_with_the_fraction_at_stake():
    assert theme.risk_color(0.1) == theme.WIN
    assert theme.risk_color(1.0) == theme.YELLOW
    assert theme.risk_color(0.5) != theme.risk_color(0.9)


# ── The rules screen reflows ────────────────────────────────────────


def test_the_rules_screen_wraps_instead_of_clipping():
    # It had hardcoded line breaks written for one width, so every narrower
    # terminal cut the prose off mid-sentence and ran the payout table off
    # both the right edge and the bottom.
    async def go():
        app, scene = game_app()
        async with await _piloted(app, size=(80, 30)) as pilot:
            await pilot.pause()
            scene.handle_key("h")
            settle(app)
            for width, height in [(80, 30), (64, 24), (50, 20), (46, 16)]:
                await pilot.resize_terminal(width, height)
                await asyncio.sleep(0.2)
                lines = buffer_text(app).split("\n")
                assert len(lines) <= height
                for line in lines:
                    assert len(line) <= width, f"overflow at {width}x{height}"
            app.game.quit()

    run(go())


def test_the_rules_screen_always_keeps_its_way_out():
    async def go():
        app, scene = game_app()
        async with await _piloted(app, size=(80, 30)) as pilot:
            await pilot.pause()
            scene.handle_key("h")
            settle(app)
            for width, height in [(80, 30), (64, 24), (50, 20)]:
                await pilot.resize_terminal(width, height)
                await asyncio.sleep(0.2)
                assert "any key to go back" in buffer_text(app)
            app.game.quit()

    run(go())


def test_the_payout_table_is_all_there_or_not_at_all():
    # Half a payout table is worse than none, so it is drawn only when the
    # remaining rows can hold every line of it.
    async def go():
        app, scene = game_app()
        async with await _piloted(app, size=(80, 30)) as pilot:
            await pilot.pause()
            scene.handle_key("h")
            settle(app)
            await asyncio.sleep(0.2)
            text = buffer_text(app)
            if "PAYOUTS" in text:
                for name in config.PAYOUT_MULTIPLIERS:
                    if name != "High Card":
                        assert name in text, f"{name} missing from the payouts"
            app.game.quit()

    run(go())
