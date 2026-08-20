# Texas Hold'Em Lava Dome

A Wii homebrew port of the MagmaCrunch arcade game, built on
[magnolia](../magnolia).

Solo Texas Hold'Em with no opponent. The dome charges an escalating ante each
round, and your hand is scored against a threshold that climbs with it: beat the
threshold and win chips, miss it and forfeit the bet. After each round you choose
to bank chips — safe, and the banked total is your score — or leave them in play.
The session ends when you go bust, or when you escape the dome voluntarily.

## Where the rules come from

The browser version, at `website/arcade/solitaire_THLD`; the directory name is
historical. Three things there are the source of truth and should not be
re-derived by eye:

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

Unlike the George Boole port, the web version here has **no test suite** to carry
over, so the rules cannot be checked by agreeing with a reference — something
else has to stand in for one.

## Layout

```
lava-dome-wii/
├── Makefile
├── meta.xml          Homebrew Channel entry
├── source/           game code
├── sprites/          PNGs, embedded into the binary by bin2s
├── audio/            raw PCM, embedded into the binary by bin2s
└── ../magnolia/      the engine, checked out beside this directory
```

## Building

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH=$DEVKITPPC/bin:$PATH

make            # build build/lava-dome.dol
make deploy     # stage sdcard/apps/lava-dome/
make dolphin    # push that to the folder Dolphin reads as its SD card
```

`make dolphin` clears the app directory rather than merging, so **saved scores
and settings are deleted on every deploy**. That is right for a dev loop and
wrong to mistake for the game failing to save.

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
