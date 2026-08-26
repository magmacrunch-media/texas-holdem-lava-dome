#ifndef LAVA_DOME_TEST_HARNESS_H
#define LAVA_DOME_TEST_HARNESS_H

/* The counting and reporting shared by the host tests. Same shape as
 * magnolia/tests/harness.h -- copied rather than included, because reaching
 * across into the engine's test directory would couple this game to the
 * engine's internals for no gain.
 *
 * `inline` rather than plain `static`, so a test that happens not to need one
 * of these still compiles clean under -Wall -Wextra.
 */

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static inline void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        printf("  FAIL: %s\n", what);
        failures++;
    }
}

static inline void check_int(int got, int want, const char *what) {
    checks++;
    if (got != want) {
        printf("  FAIL: %s (got %d, want %d)\n", what, got, want);
        failures++;
    }
}

static inline void check_str(const char *got, const char *want, const char *what) {
    checks++;
    if (!got || !want || strcmp(got, want) != 0) {
        printf("  FAIL: %s (got \"%s\", want \"%s\")\n",
               what, got ? got : "(null)", want ? want : "(null)");
        failures++;
    }
}

static inline int report(void) {
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

#endif
