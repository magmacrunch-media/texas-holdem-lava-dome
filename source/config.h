#ifndef CONFIG_H
#define CONFIG_H

/* Homebrew Channel app directory. The engine derives sd:/apps/<APP_NAME>/... from
   this, so asset and save paths are not repeated around the codebase. */
#define APP_NAME            "lava-dome"

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

#endif
