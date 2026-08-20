#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "magnolia.h"
#include "config.h"

static int frame_count = 0;

static void start_run(void) {
    frame_count = 0;
    scoring_reset();
    /* Reset your world here. */
}

static void update_playing(GameStateMachine *gs) {
    frame_count++;

    /* Your game goes here. Call gamestate_end_run(gs, scoring_get()) when it
       ends; the engine takes over from there -- game over, initials on a
       qualifying score, then the leaderboard. */
    (void)gs;

    renderer_draw_background();
    ui_draw_border();
    ui_draw_centered_text(200, "PLAYING", 24, RGBA(0, 212, 255, 255));
    renderer_finish();

#if DEBUG_HEARTBEAT_FRAMES
    if (frame_count % DEBUG_HEARTBEAT_FRAMES == 0) {
        printf("heartbeat: frame=%d score=%d\n", frame_count, scoring_get());
    }
#endif
}

static void draw_title(void) {
    renderer_draw_background();
    ui_draw_border();
    ui_draw_centered_text(180, "lava-dome", 32, RGBA(0, 212, 255, 255));
    ui_draw_centered_text(300, "Press A", 14, RGBA(160, 160, 160, 255));
    renderer_finish();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    srand(time(NULL));

    /* printf reaches Dolphin's log through the OSReport channel -- but only if
       OSREPORT and WriteToFile are enabled in Dolphin's Logger.ini. They are off
       by default, which makes a tracing session look like a silent one.
       Unbuffered, or the line written immediately before a crash dies with it. */
    SYS_STDIO_Report(true);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lava-dome starting ===\n");

    const MagnoliaConfig cfg = {
        .app_name     = APP_NAME,
        .max_scores   = HIGH_SCORE_COUNT,
        .overscan_pct = OVERSCAN_PCT
    };

    int status = magnolia_init(&cfg);
    /* -2 means video never came up; every draw below would be undefined. */
    if (status == -2) return 1;

    input_init();
    audio_init();

    GameStateMachine gs;
    gamestate_init(&gs);

#if AUTOSTART_GAMEPLAY
    printf("autostart: skipping menus, entering gameplay directly\n");
    start_run();
    gamestate_set(&gs, GS_PLAYING);
#endif

    while (1) {
        input_scan();
        if (input_home_pressed()) break;

        if (gamestate_current(&gs) == GS_PLAYING) {
            update_playing(&gs);
            continue;
        }

        switch (gamestate_current(&gs)) {
            case GS_TITLE:
                draw_title();
                break;
            /* GS_MENU, GS_READY, GS_GAME_OVER, GS_INITIALS and GS_HIGH_SCORES
               are yours to draw; the engine owns the transitions between them. */
            default:
                renderer_draw_background();
                ui_draw_border();
                renderer_finish();
                break;
        }

        /* Returns 1 the moment it enters GS_PLAYING, which is when the world
           needs resetting. */
        if (gamestate_update(&gs, scoring_get())) {
            start_run();
        }
    }

    audio_shutdown();
    magnolia_shutdown();
    return 0;
}
