#ifndef CONFIG_H
#define CONFIG_H

/* Homebrew Channel app directory. The engine derives sd:/apps/<APP_NAME>/... from
   this, so asset and save paths are not repeated around the codebase. */
#define APP_NAME            "texas-holdem-lava-dome"

#define HIGH_SCORE_COUNT    10

/* Percent of each screen edge assumed lost to TV overscan. Raise if the border or
   the bottom line of text is cut off on your set. */
#define OVERSCAN_PCT        6

/* Unattended test hooks. All off in a normal build. Reaching gameplay by hand
   needs button presses into an emulator window, which a script cannot easily
   provide, and without a heartbeat a silent log cannot tell a crash from a game
   sitting quietly on a screen with nothing left to say. Turning these on is how
   you find out whether the game runs, in one command, with no controller. */
#define AUTOSTART_GAMEPLAY      0   /* boot straight into a run */
#define DEBUG_HEARTBEAT_FRAMES  0   /* print progress every N frames; 0 off */

/* Debug-only: press A by itself every N frames.
 *
 * Every choice in this game is made with A, so a synthetic A press is enough to
 * drive a whole session -- bet, three streets, resolve, cash out, next round --
 * with no controller and no emulator input at all. Synthesising real input into
 * a Dolphin window is unreliable (Windows ignores SetForegroundWindow from a
 * background process, and DirectInput polls device state rather than window
 * messages), so the dependable way to exercise the loop on a console is to have
 * the game supply its own presses.
 *
 * It is also a soak test. Left running it plays session after session, which is
 * how anything that only breaks on the fortieth round gets found. */
#define DEBUG_AUTOPLAY_FRAMES   0

#endif
