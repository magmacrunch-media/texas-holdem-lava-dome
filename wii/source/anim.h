#ifndef ANIM_H
#define ANIM_H

/* The two shapes of motion this game needs, and nothing else.
 *
 * Every function here takes `dt` in seconds rather than calling magnolia's
 * clock_dt() for itself. That is the whole reason the file exists separately:
 * a module that reads the clock is a module that can only be checked on a
 * television, and these are exactly the things -- does a counter converge, does
 * a reveal ever finish -- that are miserable to verify by watching and trivial
 * to verify by running. Seconds rather than frames for the same reason
 * george-boole uses them: a frame count means something different on PAL.
 */

/* A number that catches up to another one. Used for the chip and bank totals,
 * which otherwise jump by hundreds between one frame and the next and give the
 * player nothing to read on the frame that matters most.
 *
 * Exponential rather than linear: winning 2,000 chips and winning 20 should
 * both feel immediate, and a fixed rate makes one crawl or the other snap. */
typedef struct {
    float current;
    float target;
    float rate;     /* fraction of the remaining gap closed per second */
} Tween;

/* Both value and target, with no motion. For a session starting over, where
   tweening from the last run's total would animate a number that was never
   real. */
void tween_set(Tween *t, float value);

void tween_to(Tween *t, float target);

/* Advances and returns the new current value. Snaps once the gap is under half
   a chip, so a counter cannot sit forever a rounding error short of its target
   and display a stack one chip lighter than the player actually has. */
float tween_step(Tween *t, float dt);

/* What a tween should read as. Rounds rather than truncates: on the way up,
   truncation shows the target one frame late and one short. */
int tween_display(const Tween *t);

/* Advances a 0..1 progress value over `seconds`, clamping at 1. Returns 1 on
 * the frame it finishes and every frame after, so callers can gate input on it
 * without tracking a separate flag.
 *
 * A `seconds` of zero completes immediately rather than dividing by it, which
 * is what makes "animations off" a single constant rather than a branch at
 * every call site. */
int anim_step(float *t, float dt, float seconds);

#endif
