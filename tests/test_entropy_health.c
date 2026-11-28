#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f0xx.h"

static TIM_TypeDef test_timer;
#undef TIM14
#define TIM14 (&test_timer)

/* Compile the real firmware logic into this test so its private health monitor
 * can receive deterministic symbols without adding a firmware test API. */
#include "../src/firmware/services/crypto/jitter_rng.c"

void SystemCoreClockUpdate(void)
{
    /* Only the real hardware initialization calls this. */
}

static void check(int passed, const char *test, int line)
{
    if (!passed) {
        fprintf(stderr, "FAIL at line %d: %s\n", line, test);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

static void test_repetition_count(void)
{
    health_reset();
    for (unsigned i = 1; i < JITTER_RCT_CUTOFF; ++i)
        CHECK(health_feed(2) == 0);
    CHECK(health_feed(2) != 0);
    printf("PASS entropy RCT (alarm at %u repeated samples)\n", JITTER_RCT_CUTOFF);
}

static void test_adaptive_proportion(void)
{
    health_reset();
    /* Insert a one every 50 samples to avoid a repetition alarm. */
    const unsigned period = 50;
    unsigned matches = 0;
    unsigned samples = 0;
    while (matches < JITTER_APT_CUTOFF && samples < JITTER_APT_WINDOW) {
        uint8_t symbol = (uint8_t)(samples % period == period - 1);
        ++samples;
        matches += symbol == 0;
        CHECK((health_feed(symbol) != 0) == (matches == JITTER_APT_CUTOFF));
    }
    CHECK(matches == JITTER_APT_CUTOFF);
    printf("PASS entropy APT (alarm at %u matches in %u samples)\n", matches, samples);
}

static void test_balanced_control(void)
{
    health_reset();
    for (unsigned i = 0; i < 2u * JITTER_APT_WINDOW; ++i)
        CHECK(health_feed((uint8_t)(i & 3u)) == 0);
    puts("PASS entropy health checks (balanced control across two windows)");
}

static void test_capture_timeout(void)
{
    /* Start in the post-initialization state with no timer capture flag. */
    s_ready = 1;
    s_failed = 0;
    test_timer.SR = 0;
    uint8_t output[32];
    const uint8_t zeros[sizeof output] = {0};
    memset(output, 0xa5, sizeof output);
    CHECK(jitter_rng_fill(output, sizeof output) == JITTER_ERR_TIMEOUT);
    CHECK(memcmp(output, zeros, sizeof output) == 0);
    CHECK(jitter_rng_fill(output, sizeof output) == JITTER_ERR_TIMEOUT);
    puts("PASS entropy capture timeout (output cleared, failure latched)");
}

int main(void)
{
    test_balanced_control();
    test_repetition_count();
    test_adaptive_proportion();
    test_capture_timeout();
    return EXIT_SUCCESS;
}
