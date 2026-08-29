# Texas Hold'Em Lava Dome — terminal version

The third sibling to `web/` (adenosine, browser) and `wii/` (magnolia, C99).
Runs entirely in a shell — no graphics, just characters.

Solo Hold'Em with no opponent. The dome charges an escalating ante each round
and scores your hand against a threshold that climbs with it. Beat it and the
stake comes back with a multiplier; miss and it is gone. Between hands you
choose to bank chips — safe, and the bank is your score — or leave them in
play for the next ante to eat.

```
pipx install magmacrunch-thld
lava-dome
```

`pipx` rather than `pip` because it puts the command on your PATH in
its own virtualenv; plain `pip install` only reaches your PATH inside an
activated venv. It is also a cabinet in the
[magmacrunch](https://pypi.org/project/magmacrunch/) arcade — `pipx install magmacrunch`
gets this and the other two — and plays identically either way.

For working on it:

```
pip install -e ".[dev]"
python -m lavadome        # or the installed `lava-dome` command
```

```
python -m lavadome --play           # skip the title screen
python -m lavadome --seed 42        # a reproducible shuffle
python -m lavadome --ascii          # suits as H/D/C/S, for fonts without ♥♦♣♠
```

Published as **`magmacrunch-thld`** — prefixed for the same reason as
`magmacrunch-george-boole`; see [that repo's `PACKAGING.md`](../../george-boole/tui/PACKAGING.md).
The import package stays plain `lavadome`.

## Keys

| | |
|---|---|
| ← → | adjust the bet |
| `1`–`4` | quick bet amounts |
| `B` / `C` | bet / check for free (pre-flop) |
| Space | take the next card |
| `R` / `F` | raise / fold |
| `K`, `1`–`4` | bank all / bank some |
| `W` | withdraw from the bank |
| `N` / `E` | next round / escape with what you banked |
| `H`, `Esc`, `Q` | help, back, quit |

A raise buys the next card, so a hand holds four betting decisions and never
more. That is the web build's behaviour and the Wii port's; letting a raise
stay on its street would be an open betting loop, which is a longer and
different game.

Needs a terminal at least 58x22.

## High scores

Kept on disk, so a record outlives the session. Filed under `solitaire-thld` —
the key the browser build has used since before the rename — and recording the
same quantity the browser does: **total wealth**, `chips + bank`, not the bank
alone. Filing a different number under the same name is the sort of thing
nobody notices until the numbers are wrong.

The moment that counts is leaving the dome, not losing a hand. Both ways out
reach it: escaping banks the chips, and busting on the ante keeps whatever was
banked already. Each entry keeps the rounds survived and whether you got out,
the same two extras the browser stores.

## Launchable by an arcade

The game declares itself through an entry point, so anything enumerating
`magmacrunch.games` finds it:

```toml
[project.entry-points."magmacrunch.games"]
thld = "lavadome.arcade:GAME"
```

It does not own the terminal. A `texastoast.core.tui_host.TuiHost` does, and
`LavaDomeApp` is handed one — which is what lets the same code run as its own
command and be seated by a launcher without knowing which happened. Esc from
the title screen ends a standalone session and returns to the arcade menu under
a launcher; the game just pops a scene and the host decides what that means.

## How it is built

```
lavadome/
  cards.py     Card and Deck — ace-high, no rendering
  handeval.py  the poker evaluator
  config.py    the tuned numbers: antes, thresholds, payouts
  state.py     session and round state
  dealer.py    the deck and the four streets
  dome.py      ante, resolution, bust and escape
  betting.py   bet sizing, raises, banking
  theme.py     palette and card drawing, in character cells
  scenes.py    TitleScene, RulesScene, GameScene
  app.py       wiring: the game, the renderer, the scene stack
```

Everything above `theme.py` imports nothing outside the standard library — not
texastoast, not Textual. A test enforces it. The engine is
[texastoast](../../texastoast) with its terminal backend, and the game draws
through its `Renderer`/`UISurface` protocols rather than against Textual, so
the planned hand-written ANSI backend will be a swap and not a rewrite.

Modality is the scene stack, not a flag: `TitleScene` sits at the bottom, a run
pushes over it, and the rules screen pushes over whichever is showing.

## Verifying the evaluator

The web build has **no test suite** — the Wii port's README calls that out as a
problem, since it means the rules cannot be checked by agreeing with a
reference.

`tools/js_oracle.mjs` stands in for one. It loads the actual shipped
`arcade/shared/adenosine-cards.js` bundle in node, and `tests/test_handeval.py`
runs both evaluators over thousands of random hands, comparing name, rank,
points, tiebreakers and description on every one. That is the same method
`web/js/config.js` records using when AdCards replaced this game's original
evaluator: *"Verified behaviourally identical over 20,000 random 2-7 card
hands."*

Those tests skip when node is absent; the hand-written cases still run.

## Deliberate differences from `web/`

Both match what the Wii port already does, and both are recorded in
[`wii/README.md`](../wii/README.md).

- **Aces are high at construction**, so there is no restamp step to forget.
  The web build deals from an ace-*low* `Deck` and restamps every card in
  `Dealer._draw()`, because the evaluator reads `value` off a card and never
  rewrites it. Get this wrong and a royal flush grades as an ordinary flush.
- **No burn cards.** The web build discards one before the flop, turn and
  river. Off a freshly shuffled deck that cannot move any odds. It would matter
  only for replaying a seed against the browser, which is impossible anyway —
  the two use different random number generators.

One further difference, which is a fix rather than a taste call:

- **Hands resolve on the phase transition, not during a render.** The web build
  calls `resolveHand()` from `_phaseResolve()`, a render function, and gets away
  with it only because resolving moves the phase on. A terminal redraws on
  every resize, so awarding chips from a render would be a live bug here.
