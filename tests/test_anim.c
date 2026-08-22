/* Motion, checked by running it rather than by watching it.
 *
 * Both of these fail in ways that are hard to see and easy to ship: a tween
 * that rests a chip short of its target shows the player a stack they do not
 * have, and a reveal that never reaches 1.0 leaves input gated forever -- a
 * game that looks alive and takes no buttons. Neither is visible in a
 * screenshot, and both are one loop away from certain here.
 *
 *   make test-anim
 */
#include <stdio.h>
#include "harness.h"
#include "anim.h"

/* A 60Hz frame. Every convergence test below steps in these so the counts mean
   something -- "arrives within a second" rather than "arrives eventually". */
#define FRAME (1.0f / 60.0f)

static void test_tween_converges(void) {
    printf("anim: a counter catching up\n");

    Tween t;
    tween_set(&t, 0.0f);
    check_int(tween_display(&t), 0, "a fresh tween reads as its value");

    tween_to(&t, 500.0f);
    check_int(tween_display(&t), 0, "and does not jump the moment it is retargeted");

    /* The property that matters: it gets there, and within a time a player
       would call immediate. A rate that is too low fails here rather than
       being noticed a month later as "the chips feel sluggish". */
    int frames = 0;
    while (tween_display(&t) != 500 && frames < 600) {
        tween_step(&t, FRAME);
        frames++;
    }
    check(frames < 60, "500 chips land inside a second");
    check_int(tween_display(&t), 500, "and land exactly, not near");

    /* Downward too -- the bank falls when chips are withdrawn from it. */
    tween_to(&t, 120.0f);
    frames = 0;
    while (tween_display(&t) != 120 && frames < 600) {
        tween_step(&t, FRAME);
        frames++;
    }
    check_int(tween_display(&t), 120, "and it comes down as readily as it goes up");

    /* Stepping a settled tween must not drift it back off the target. */
    for (int i = 0; i < 100; i++) tween_step(&t, FRAME);
    check_int(tween_display(&t), 120, "a settled tween stays settled");
}

static void test_tween_survives_a_bad_frame(void) {
    printf("anim: a frame that took too long\n");

    /* A long frame -- the first one after an SD read -- must not overshoot and
       spring back. Without the clamp on the step fraction this lands past the
       target and walks back to it, which reads as an arithmetic bug rather
       than as the hitch it actually is. */
    Tween t;
    tween_set(&t, 0.0f);
    tween_to(&t, 100.0f);
    tween_step(&t, 5.0f);
    check(tween_display(&t) <= 100, "a five-second frame does not overshoot");
    check_int(tween_display(&t), 100, "it simply arrives");

    /* Zero dt is a paused frame, not a teleport. */
    Tween z;
    tween_set(&z, 0.0f);
    tween_to(&z, 100.0f);
    tween_step(&z, 0.0f);
    check_int(tween_display(&z), 0, "a zero-length frame moves nothing");

    /* Negative values are real here: the bank never goes below zero, but the
       rounding in tween_display() has to be right in both directions or a
       counter passing through zero reads one off. */
    Tween n;
    tween_set(&n, -4.4f);
    check_int(tween_display(&n), -4, "rounding is symmetric about zero");
    tween_set(&n, -4.6f);
    check_int(tween_display(&n), -5, "in both directions");
}

static void test_anim_step_finishes(void) {
    printf("anim: a reveal that ends\n");

    /* Input is gated on this returning 1. If it can fail to, the game stops
       taking buttons while still drawing -- the worst failure this file can
       catch, because it looks exactly like a running game. */
    float t = 0.0f;
    int done = 0;
    int frames = 0;
    while (!done && frames < 600) {
        done = anim_step(&t, FRAME, 0.25f);
        frames++;
    }
    check(done, "a quarter-second reveal finishes");
    check(frames <= 16, "in about the frames a quarter second holds");
    check(t == 1.0f, "and lands exactly on 1, not past it");

    /* Once finished it stays finished, so a caller can gate on it without
       keeping a second flag beside it. */
    check(anim_step(&t, FRAME, 0.25f), "and reports finished on every frame after");
    check(t == 1.0f, "without climbing past 1");

    /* Zero seconds is how animation gets turned off at a call site: it
       completes on the first step rather than dividing by zero. */
    float z = 0.0f;
    check(anim_step(&z, FRAME, 0.0f), "a zero-length animation is already over");
    check(z == 1.0f, "and reads as complete");

    /* A single enormous frame must also land on 1 rather than beyond it --
       the same overshoot the tween guards against, in the other module. */
    float big = 0.0f;
    anim_step(&big, 10.0f, 0.25f);
    check(big == 1.0f, "one long frame finishes it without overshooting");
}

int main(void) {
    test_tween_converges();
    test_tween_survives_a_bad_frame();
    test_anim_step_finishes();
    return report();
}
