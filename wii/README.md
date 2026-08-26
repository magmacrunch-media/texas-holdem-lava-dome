# Texas Hold'Em Lava Dome

A Wii homebrew port of the magmacrunch arcade game, built on
[magnolia](../../magnolia).

Solo Texas Hold'Em with no opponent. The dome charges an escalating ante each
round, and your hand is scored against a threshold that climbs with it: beat the
threshold and win chips, miss it and forfeit the bet. After each round you choose
to bank chips — safe, and the banked total is your score — or leave them in play.
The session ends when you go bust, or when you escape the dome voluntarily.

## Where the rules come from

The browser version, now in this repo at [`../web/`](../web/) (served from the
website as `arcade/solitaire_THLD` — that directory name is historical). Three
things there are the source of truth and should not be re-derived by eye:

| What | Where |
|---|---|
| Tuned numbers: ante schedule, thresholds, payouts | `js/config.js` |
| Round flow, bust and escape conditions | `js/dome.js`, `js/state.js` |
| Hand evaluation | `arcade/shared/adenosine-cards.js` (`AdCards.HandEvaluator`) |

Two details are easy to port wrong, and both produce a game that plays almost
right — which is worse than one that plainly does not:

- **Aces are high.** `AdCards.RANK_VALUES` is ace-*low* (A = 1) and the evaluator
  reads `value` straight off the card, so the web build restamps every dealt card
  ace-high in `Dealer._draw()`. This port defines ace-high once, in `cards.h`.
- **High Card pays nothing.** Its payout multiplier is 0, so a high card cannot
  beat the dome at any threshold.

Two further places this port and the web build differ, both on purpose:

- **A raise buys the next card.** `js/ui.js` calls `betting.raise()` and
  `dealer.advanceStreet()` on the same click, so a hand holds four betting
  decisions -- the pre-flop bet and one at each street -- and never more. This
  port does the same. Letting a raise stay on its street would be an open
  betting loop the arcade version does not have, which is a different game and
  a longer one.
- **No burn cards.** The web build discards one card before the flop, the turn
  and the river. Off a freshly shuffled deck that cannot move any odds, so it is
  not reproduced; `dealer.h` says so at the point where it would go. It would
  matter only for replaying a seed against the browser, which nothing does.

Unlike the George Boole port, the web version here has **no test suite** to carry
over, so the rules cannot be checked by agreeing with a reference — something
else has to stand in for one.

## Layout

```
texas-holdem-lava-dome-wii/
├── Makefile
├── meta.xml          Homebrew Channel entry
├── source/           game code
├── sprites/          PNGs, embedded into the binary by bin2s
├── audio/            raw PCM, embedded into the binary by bin2s
└── ../../magnolia/   the engine, checked out beside the repo
```

## Building

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH=$DEVKITPPC/bin:$PATH

make            # build build/texas-holdem-lava-dome.dol
make deploy     # stage sdcard/apps/texas-holdem-lava-dome/
make dolphin    # push that to the folder Dolphin reads as its SD card
```

`make dolphin` clears the app directory rather than merging, so **saved scores
and settings are deleted on every deploy**. That is right for a dev loop and
wrong to mistake for the game failing to save.

## Onto a real Wii

Two routes, and they do different jobs.

```bash
make card SD=/mnt/e             # install onto an SD card (permanent)
make wii  WIILOAD=tcp:<wii-ip>  # send this build to a running console (temporary)
```

**`make card`** is the one that installs the game. It needs the card's mount
point because a removable drive's letter moves — a card showing as `E:` in
Windows is `/mnt/e` in WSL. Eject it, put it in the console, and the game appears
in the Homebrew Channel under the name in `meta.xml`.

Unlike `make dolphin`, this **merges**: only `boot.dol` and `meta.xml` are
overwritten, so `scores.json` and `settings.json` on the card survive an update.
Wiping is right for a dev loop against an emulator and wrong when it is somebody's
high scores.

**`make wii`** sends the `.dol` over the network and runs it immediately, without
installing anything. It is the fast loop for real hardware — no card, no ejecting,
a couple of seconds. Open the Homebrew Channel, press Home for the netloader
screen, and use the IP address it displays:

```bash
export WIILOAD=tcp:192.168.1.50   # once per shell, then just `make wii`
```

The console has to be sitting on that netloader screen when you send. Nothing is
written to the card, so the game is gone when you quit it.

## Assets

Sprites and audio are linked into the binary, not read from the card: assets on a
card can go missing, go stale, or disagree with the code.

Audio is raw signed 16-bit little-endian PCM:

```bash
ffmpeg -i in.ogg -f s16le -acodec pcm_s16le -ar 48000 -ac 2 audio/track.pcm
```

Budget the memory before committing to a format. Clips are held decoded in RAM:

| Format | Rate | One minute |
|---|---|---|
| 48kHz stereo | ~192 KB/s | ~11.5 MB |
| 24kHz mono | ~48 KB/s | ~2.9 MB |

Against the Wii's 24MB, a long track at 48kHz stereo does not fit. Downsample the
music loop with `audio_play_music_fmt()`; keep short effects at 48kHz stereo.

## Rendering

Cards are drawn, not loaded — there are no image assets. The face is one of
magnolia's rounded panels, the rank is text, and the suit is a pip built from
GRRLIB primitives, because Press Start 2P has no suit glyphs and a missing glyph
on a CRT is indistinguishable from a bug.

`GRRLIB_NGoneFilled` is a triangle fan and so is only honest about convex
shapes. A diamond is therefore one polygon, while hearts, spades and clubs are
composed from circles plus a triangle. Colour carries the suit redundantly, so a
pip that softens at distance still reads red or black.

Cards slide the last few pixels into their slots and fade up rather than
scaling into place. Scaling would mean drawing a rank at fractional sizes on
every frame of the animation, and Press Start 2P is a pixel font -- that is the
same smudge the drop-shadow workaround below exists to avoid, arriving by
another route. Sliding leaves every glyph at the one size it was drawn for.

**One magnolia limitation found here.** Every text helper in `ui_utils` draws a
hardcoded black drop shadow two pixels down and right. That suits the games it
was written for, which put light text on dark backgrounds — and it wrecks a red
rank on a pale card face, turning a Q into a smudge. This game draws card ranks
with `GRRLIB_PrintfTTF` directly instead. A shadow-colour argument would fix it
for everyone, but one consumer is not two, so it stays here until a second game
wants a light background. It is the first real extraction candidate this port
has produced.

## Testing

```bash
make test    # needs only a C compiler; no devkitPPC, no emulator
```

The rules are plain C with no libogc in them, so they run on the machine you are
sitting at. `make test` deliberately does **not** depend on the `.dol` build —
depending on it would drag devkitPPC into the one target that exists for not
needing it.

**The evaluator is checked against arithmetic, not against itself.** There is no
reference implementation to agree with, and a hand-written list of poker cases
only proves the evaluator matches whatever the person writing the cases believed
— which, for the wheel and the ace-high straight, is exactly the thing in doubt.
So the test deals every five-card hand that exists, classifies each one, and
checks the ten totals against the published frequencies:

| Hand | Count |
|---|---|
| Royal flush | 4 |
| Straight flush | 36 |
| Four of a kind | 624 |
| Full house | 3,744 |
| Flush | 5,108 |
| Straight | 10,200 |
| Three of a kind | 54,912 |
| Two pair | 123,552 |
| One pair | 1,098,240 |
| High card | 1,302,540 |

Those are properties of a 52-card deck, not of this code. They sum to 2,598,960,
so the total is self-checking too, and the whole sweep takes about a second.

It is a sharp instrument. Removing just the wheel case from the straight
detection moves four of the ten counts — straights fall by 1,020 and straight
flushes by 4, because 1,024 wheels exist and four of them are suited — and the
failure names every one.

Seven cards is 133,784,560 combinations, too many for every build. That path is
covered by a property instead: the best hand out of seven can never rank below
any five of those seven, checked over 20,000 seeded deals against all 21 subsets
of each.

`make test` builds three binaries, one per file, each naming what it links. The
third is `test-anim`, and it is there because both of the things `source/anim.c`
does fail invisibly. A counter that rests half a chip short of its target shows
a stack the player does not have, and a reveal that never reaches 1.0 leaves
input gated forever -- a game that draws every frame and takes no buttons.
Neither shows up in a screenshot. Both are one loop away from certain here,
which is why the module takes `dt` as an argument instead of reading magnolia's
clock: a module that reads the clock can only be checked on a television.

## Testing without a controller

Reaching gameplay by hand needs button presses into an emulator window. Instead:

```c
#define AUTOSTART_GAMEPLAY      1
#define DEBUG_HEARTBEAT_FRAMES  120
```

Then run the `.dol` and read the log. For `printf` to reach Dolphin's log, its
`Logger.ini` needs `OSREPORT = True` and `WriteToFile = True` -- both default to
False, which makes a working trace look like a dead one.

Dolphin also reuses an already-running instance, so kill it between runs or you
will read the previous run's log and debug a binary that is not running.

An autoplay build does two things a normal one does not, both so that it can
keep playing rather than stopping somewhere that looks like progress:

- **It skips the initials editor.** That screen and the leaderboard behind it
  are driven by magnolia's shell, which reads the real Wiimote -- there is no
  press to synthesise into them. Every soak run scores, and every score is a
  high score on a card `make dolphin` has just cleared, so without the skip the
  soak plays exactly one session and then sits on the initials screen forever
  with a log that looks perfectly healthy. Runs are filed as `AAA`, which also
  puts `scoring_add_entry()` and the save on the exercised path.
- **It picks buttons at random.** Stepping the cursor by the press number was
  the obvious thing and is quietly useless: the press number advances in
  lockstep with the phases, so the same index lands on the same button every
  session. With a four-button street menu that index was FOLD -- six hands out
  of six folded, no showdown ever reached, and a log full of activity. Random
  costs a seeded session its reproducibility, and only in this build, which is
  a fair price for covering more than one path.

## License

Under the [PolyForm Noncommercial License 1.0.0](../LICENSE), like the rest of
the repository — the hand evaluator and the sweep that proves it, the dome's
ante and threshold model, and the procedural card rendering are all there to
read and build on for any noncommercial purpose. Commercial use is reserved.

The game's name, the lava palette and visual design, the depth labels, and any
art or audio added later are © 2026 magmacrunch media, all rights reserved, and
are **not** covered by that licence. Nothing of that kind ships today — the cards
are drawn, not loaded. See [NOTICE](../NOTICE) for the exact boundary and the
third-party components.
