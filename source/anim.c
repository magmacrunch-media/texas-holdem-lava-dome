#include "anim.h"

/* How much of the remaining gap a tween closes each second. Tuned by eye
   against the chip counter: high enough that banking 500 chips reads as an
   action rather than a wait, low enough that the number is legibly moving
   rather than flickering once and stopping. */
#define TWEEN_RATE   12.0f

/* Below this the tween is done. Half a chip, because the display rounds -- a
   gap smaller than that is already showing the right number, and continuing to
   step it only risks the counter resting a chip short of the truth. */
#define TWEEN_EPSILON 0.5f

void tween_set(Tween *t, float value) {
    t->current = value;
    t->target  = value;
    t->rate    = TWEEN_RATE;
}

void tween_to(Tween *t, float target) {
    t->target = target;
    if (t->rate <= 0.0f) t->rate = TWEEN_RATE;
}

float tween_step(Tween *t, float dt) {
    float gap = t->target - t->current;

    if (gap < 0.0f) gap = -gap;
    if (gap <= TWEEN_EPSILON) {
        t->current = t->target;
        return t->current;
    }

    /* Fraction of the gap to close this frame. Clamped at 1 because a long
       frame -- the first one after an SD read, say -- would otherwise overshoot
       past the target and spring back, which looks like a bug in the arithmetic
       rather than a hitch in the frame time. */
    float k = t->rate * dt;
    if (k > 1.0f) k = 1.0f;
    if (k < 0.0f) k = 0.0f;

    t->current += (t->target - t->current) * k;
    return t->current;
}

int tween_display(const Tween *t) {
    float v = t->current;
    return (int)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

int anim_step(float *t, float dt, float seconds) {
    if (seconds <= 0.0f) {
        *t = 1.0f;
        return 1;
    }

    *t += dt / seconds;
    if (*t >= 1.0f) {
        *t = 1.0f;
        return 1;
    }
    return 0;
}
