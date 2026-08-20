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
