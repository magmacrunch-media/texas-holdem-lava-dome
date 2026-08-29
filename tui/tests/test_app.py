"""The screens, driven headlessly.

What `test_rules.py` and `test_handeval.py` cannot cover: that the rules are
wired to the screen, and that the title, rules and game screens hand off to
each other correctly. Needs the engine and its terminal extra; skipped without
them so the rule tests still run on a bare checkout.
"""

import asyncio

import pytest

pytest.importorskip("textual", reason='needs: pip install -e ".[dev]"')

from magmacrunch.engine import scores as score_mod  # noqa: E402
from magmacrunch.engine.core.tui_host import TuiHost  # noqa: E402
from magmacrunch.engine.ui import bigtext  # noqa: E402

from lavadome import (  # noqa: E402
    config,
    scenes,  # noqa: E402
    theme,
)
from lavadome.app import LavaDomeApp  # noqa: E402
from lavadome.arcade import GAME  # noqa: E402
from lavadome.cards import Card  # noqa: E402
from lavadome.scenes import GameScene, RulesScene, TitleScene  # noqa: E402


@pytest.fixture(autouse=True)
def isolated_scores(tmp_path, monkeypatch):
    """No test may touch a real player's score file.

    Autouse rather than opt-in: a suite that can quietly delete somebody's high
    scores is not one you want to run twice.
    """
    monkeypatch.setenv(score_mod.DATA_DIR_ENV, str(tmp_path))


def buffer_text(app: LavaDomeApp) -> str:
    return app.host.game.surface.buffer.to_text()


def settle(app: LavaDomeApp) -> None:
    """Apply the stack's pending push/pop, which the engine defers a frame."""
    app.host.stack.update(0.0)


def hosted(seed=2024) -> LavaDomeApp:
    """A session on a real host, built the way the standalone command does."""
    host = TuiHost(title=GAME.info.title, fps=GAME.info.fps,
                   hold_ms=GAME.info.hold_ms)
    app = LavaDomeApp(host, seed=seed)
    host.push_scene(app.root_scene)
    settle(app)
    return app


def game_app(seed=2024) -> tuple[LavaDomeApp, GameScene]:
    app = hosted(seed)
    app.start_run()
    settle(app)
    return app, app.host.scene


def hand(*specs: str) -> list[Card]:
    suits = {"s": "spades", "h": "hearts", "d": "diamonds", "c": "clubs"}
    return [Card(suits[spec[-1]], spec[:-1]) for spec in specs]


async def _piloted(app: LavaDomeApp, size=(80, 24)):
    from magmacrunch.engine.core.tui_game import _GameApp

    textual_app = _GameApp(app.host.game, app.host.game.surface)
    app.host.game._app = textual_app
    return textual_app.run_test(size=size)


def run(coro):
    return asyncio.run(coro)


# ── The stack ───────────────────────────────────────────────────────


def test_the_title_screen_is_the_bottom_of_the_stack():
    app = hosted(seed=1)
    assert isinstance(app.host.scene, TitleScene)
    assert not app.in_game


def test_descending_pushes_a_run_over_the_title():
    app = hosted(seed=1)
    app.host.scene.handle_key("enter")
    settle(app)
    assert app.in_game
    assert len(app.host.stack) == 2
    assert isinstance(app.host.stack.scenes[0], TitleScene)


def test_escape_returns_to_the_title():
    app, scene = game_app()
    scene.handle_key("escape")
    settle(app)
    assert isinstance(app.host.scene, TitleScene)


def test_the_title_menu_works_again_after_a_run_is_popped():
    app = hosted(seed=1)
    app.host.scene.handle_key("enter")
    settle(app)
    app.host.scene.handle_key("escape")
    settle(app)
    assert app.host.scene.menu.active


def test_the_rules_screen_pushes_over_a_run_and_any_key_dismisses_it():
    app, scene = game_app()
    scene.handle_key("h")
    settle(app)
    assert isinstance(app.host.scene, RulesScene)

    app.host.scene.handle_key("x")
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

    app.host.stack.dispatch_key("b")         # would place a bet if it reached
    settle(app)
    assert scene.state.chips == chips
    assert app.in_game                  # the rules screen is gone

    app.host.stack.dispatch_key("b")         # now it reaches
    assert scene.state.chips < chips


def test_a_run_stops_receiving_frames_while_the_rules_are_on_top():
    app, scene = game_app()
    scene.handle_key("h")
    settle(app)
    assert app.host.scene is not scene
    # Modality is the stack: RulesScene sets no update_below, so the run
    # underneath is not in the update slice at all.
    assert scene not in app.host.stack._slice("update_below")


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
        app = hosted(seed=1)
        async with await _piloted(app) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            text = buffer_text(app)
            # Both halves of the name, drawn, broken the way the website
            # breaks it: TEXAS HOLD'EM over LAVA DOME.
            assert bigtext.lines("TEXAS HOLD'EM")[0].strip() in text
            assert bigtext.lines("LAVA DOME")[0].strip() in text
            assert theme.SUBTITLE in text
            assert "DESCEND INTO THE DOME" in text
            assert "HOW TO PLAY" in text
            app.host.quit()

    run(go())


def test_a_short_terminal_gets_the_plain_banner_instead_of_block_letters():
    """A title that pushed the menu off the screen would be a title nobody
    could get past."""
    async def go():
        app = hosted(seed=1)
        async with await _piloted(app, size=(theme.MENU_MIN_COLS,
                                             theme.MENU_MIN_ROWS)) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            text = buffer_text(app)
            assert theme.BANNER in text
            assert bigtext.lines("LAVA DOME")[0].strip() not in text
            assert "DESCEND INTO THE DOME" in text, "the menu is still reachable"
            app.host.quit()

    run(go())


def test_every_rung_of_the_ladder_spells_the_whole_name():
    """A title that fits by dropping half of itself is not the title."""
    for big, rest in theme.TITLE_LADDER:
        spelled = (big.replace('\n', " ") + " " + rest).split()
        assert spelled == theme.BANNER.split(), f"{big!r} + {rest!r}"


def test_the_break_is_what_makes_the_name_fit():
    """On one line the name is 114 columns in block letters. On two it is 64,
    which is why the website breaks it and why this does."""
    assert bigtext.width(theme.BANNER) > 100
    assert bigtext.fits(theme.TITLE_LADDER[0][0], 78, rows=6)


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
            app.host.quit()

    run(go())


def test_glyphs_keep_the_background_of_the_card_they_sit_on():
    # The compositing bug George Boole caught: a glyph must not punch a hole
    # through the card face it is printed on.
    async def go():
        app, _ = game_app()
        async with await _piloted(app) as pilot:
            await pilot.pause()
            await asyncio.sleep(0.25)
            buf = app.host.game.surface.buffer
            holes = [(x, y) for y in range(buf.height) for x in range(buf.width)
                     if buf.get(x, y).char != " " and buf.get(x, y).bg is None]
            assert holes == []
            app.host.quit()

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
            app.host.quit()

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
            app.host.quit()

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
            app.host.quit()

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
            app.host.quit()

    run(go())


# ── Card drawing ────────────────────────────────────────────────────


def test_a_card_shows_its_rank_twice_mirrored():
    from magmacrunch.engine.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("spades", "A"))
    lines = r.to_text().split("\n")
    assert lines[0].startswith("A♠")
    assert lines[2].rstrip().endswith("A♠")


def test_a_ten_still_fits_the_card():
    from magmacrunch.engine.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("diamonds", "10"))
    assert "10♦" in r.to_text()


def test_ascii_mode_swaps_the_suit_glyphs_for_letters():
    from magmacrunch.engine.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, Card("hearts", "K"), ascii_only=True)
    text = r.to_text()
    assert "KH" in text
    assert "♥" not in text


def test_an_undealt_slot_draws_nothing_but_a_background():
    from magmacrunch.engine.render.tui import TuiRenderer

    r = TuiRenderer(10, 4)
    theme.draw_card(r, 0, 0, None)
    assert r.to_text().strip() == ""
    assert r.buffer.get(0, 0).bg == theme.CARD_SLOT


def test_the_board_always_shows_five_positions():
    from magmacrunch.engine.render.tui import TuiRenderer

    r = TuiRenderer(40, 4)
    width = theme.draw_card_row(r, 0, 0, hand("As", "Kh"), slots=5)
    assert width == 5 * theme.CARD_W + 4 * theme.CARD_GAP
    # The two dealt cards are face up, the other three are empty slots.
    assert r.buffer.get(0, 0).bg == theme.CARD_FACE
    assert r.buffer.get(4 * (theme.CARD_W + theme.CARD_GAP), 0).bg == theme.CARD_SLOT


def test_red_and_black_suits_are_inked_differently():
    from magmacrunch.engine.render.tui import TuiRenderer

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
            app.host.quit()

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
            app.host.quit()

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
            app.host.quit()

    run(go())


# ── Launchable by an arcade ─────────────────────────────────────────
#
# The game has to work two ways: as its own command, and seated by a launcher
# that owns the terminal. The difference must be invisible to the game.


class _Blank:
    """Stands in for whatever a launcher would have underneath a game."""

    def update(self, dt):
        pass

    def render(self):
        pass


def test_the_entry_point_object_is_a_valid_arcade_game():
    from magmacrunch.engine.arcade import ArcadeGame

    assert isinstance(GAME, ArcadeGame)


def test_the_declared_info_matches_what_the_game_actually_needs():
    assert GAME.info.key == "thld"
    assert GAME.info.min_cols == theme.MIN_COLS
    assert GAME.info.min_rows == theme.MIN_ROWS
    # Turn-based: edge input, or one arrow press runs the bet across the stack.
    assert GAME.info.hold_ms == 0


def test_starting_returns_a_scene_without_pushing_it():
    host = TuiHost(title="t", fps=20)
    scene = GAME.start(host)
    host.stack.update(0)
    assert isinstance(scene, TitleScene)
    assert host.scene is None, "start() must leave the pushing to the caller"


def test_a_launcher_can_seat_the_game_over_its_own_menu():
    host = TuiHost(title="arcade", fps=20)
    arcade_menu = _Blank()
    host.push_scene(arcade_menu)
    host.seat(GAME)
    host.stack.update(0)
    assert isinstance(host.scene, TitleScene)

    host.scene.handle_key("escape")
    host.stack.update(0)
    assert host.scene is arcade_menu


def test_escape_from_the_title_ends_a_standalone_session():
    host = TuiHost(title="t", fps=20)
    quit_calls = []
    host.quit = lambda: quit_calls.append(1)
    host.seat(GAME)
    host.stack.update(0)

    host.scene.handle_key("escape")
    assert quit_calls == [1]


def test_seating_applies_the_declared_frame_rate_and_input():
    from magmacrunch.engine.core.loop import GameLoop
    from magmacrunch.engine.core.scheduler import ManualScheduler

    host = TuiHost(title="t", fps=60, hold_ms=999)
    host._game._loop = GameLoop(ManualScheduler(), lambda dt: None,
                                lambda: None, fps=60)
    host.seat(GAME)
    assert round(host.game.loop.target_fps) == GAME.info.fps
    assert host.input.hold_ms == GAME.info.hold_ms


def test_a_seated_game_deals_a_hand():
    host = TuiHost(title="t", fps=20)
    host.push_scene(_Blank())
    host.seat(GAME)
    host.stack.update(0)

    host.scene.handle_key("enter")          # descend into the dome
    host.stack.update(0)
    assert isinstance(host.scene, GameScene)

    scene = host.scene
    assert len(scene.state.hole_cards) == 2
    scene.handle_key("b")
    for _ in range(3):
        scene.handle_key("space")
    assert scene.resolution is not None


def test_the_rules_screen_still_returns_to_the_title_under_a_launcher():
    # Three deep: arcade menu, title, rules. Each pop goes back one.
    host = TuiHost(title="arcade", fps=20)
    arcade_menu = _Blank()
    host.push_scene(arcade_menu)
    host.seat(GAME)
    host.stack.update(0)
    title = host.scene

    host.scene.handle_key("h")
    host.stack.update(0)
    assert isinstance(host.scene, RulesScene)

    host.scene.handle_key("x")
    host.stack.update(0)
    assert host.scene is title

    host.scene.handle_key("escape")
    host.stack.update(0)
    assert host.scene is arcade_menu

# ── High scores ─────────────────────────────────────────────────────


def test_the_board_is_filed_under_the_key_the_browser_uses():
    """solitaire-thld, not thld: the web has filed it that way since before
    the rename, and a shared board later has to mean a shared board."""
    assert LavaDomeApp.SCORE_KEY == "solitaire-thld"


def test_what_goes_on_the_board_is_total_wealth_not_the_bank():
    """js/state.js records totalScore: this.totalWealth, which is chips plus
    bank. Recording the other one would put a different quantity under the
    same name — the kind of thing nobody notices until the numbers are wrong."""
    app, scene = game_app()
    scene.state.bank = 300
    scene.state.chips = 45
    scene._end({"escaped": True})
    settle(app)
    if not app.in_game:
        app.host.scene.handle_key("enter")
        settle(app)
    assert app.scores.best() == 345


def test_a_run_records_the_rounds_and_whether_they_got_out():
    """The same two extras the browser keeps."""
    app = hosted()
    app.record(500, "jam", rounds=7, escaped=True)
    assert app.scores.load()[0].extra == {"rounds": 7, "escaped": True}


def test_the_score_survives_the_process(tmp_path):
    from magmacrunch.engine.scores import ScoreBook

    book = ScoreBook(LavaDomeApp.SCORE_KEY, directory=tmp_path)
    LavaDomeApp(TuiHost(title="t"), scores=book).record(900, "jam")

    again = LavaDomeApp(TuiHost(title="t"),
                        scores=ScoreBook(LavaDomeApp.SCORE_KEY,
                                         directory=tmp_path))
    assert again.best == 900


def test_busting_records_what_was_already_banked():
    """A run that busts on the ante keeps whatever it banked. Both ways out of
    the dome reach finish()."""
    app, scene = game_app()
    scene.state.bank = 120
    scene.state.chips = 0
    scene._end({"bust": True})
    settle(app)
    if not app.in_game:
        app.host.scene.handle_key("enter")
        settle(app)
    assert app.scores.best() == 120


def test_leaving_with_nothing_is_not_worth_asking_about():
    app, scene = game_app()
    scene.state.bank = 0
    scene.state.chips = 0
    scene._end({"bust": True})
    settle(app)
    assert not isinstance(app.host.scene, scenes.InitialsScene)
    assert app.scores.load() == []


def test_the_in_run_peak_is_not_the_score():
    """record_bank tracks the highest the bank got during a run, for the HUD.
    Only what you leave with goes on the board."""
    app, scene = game_app()
    app.record_bank(9999)
    assert app.best_bank == 9999
    assert app.scores.load() == [], "the peak must not reach the board"


def test_the_title_offers_the_table():
    app = hosted()
    assert app.host.scene.ITEMS == ("DESCEND INTO THE DOME", "HIGH SCORES",
                                    "HOW TO PLAY", "QUIT")


def test_the_table_shows_the_run_and_how_it_ended():
    app = hosted()
    app.scores.save("jam", 1250, rounds=9, escaped=True)
    app.scores.save("cpr", 80, rounds=2, escaped=False)

    async def go():
        async with await _piloted(app) as pilot:
            await pilot.pause()
            app.show_scores()
            settle(app)
            await asyncio.sleep(0.3)
            text = buffer_text(app)
            assert "JAM" in text and "1250" in text
            assert "escaped" in text and "burned" in text
            app.host.quit()

    run(go())


# ── Saying where Esc goes ───────────────────────────────────────────


def _seated():
    """The cabinet seated over a floor, the way the arcade does it."""
    host = TuiHost(title="arcade", fps=20)
    floor = _Blank()
    host.push_scene(floor)
    host.stack.update(0)
    host.seat(GAME)
    host.stack.update(0)
    return host, floor


def test_a_seated_cabinet_says_how_to_get_back():
    host, _ = _seated()
    host.scene.render()
    assert scenes.ARCADE_HELP in host.game.surface.buffer.to_text()


def test_a_game_launched_on_its_own_does_not_promise_an_arcade():
    # The same screen, the same key, a different truth: Esc ends the session
    # here, and Q already says so.
    app = hosted()
    app.host.scene.render()
    assert scenes.ARCADE_HELP not in buffer_text(app)
