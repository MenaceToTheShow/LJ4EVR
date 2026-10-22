/* test_lock.c - lj_lock/lj_unlock pair, and the 3.1 hold-time line: a 3 ms hold
 * is logged when the measurement is compiled in (LJ_DEBUG or no NDEBUG). */
#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_clock.h"
#include "core/lj_log.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int s_hold_lines;
static void lj_capture(void *ud, const char *line)
{
    (void)ud;
    if (strncmp(line, "lock: held", 10) == 0) s_hold_lines++;
}

int lj_test_lock(int argc, char **argv);
int lj_test_lock(int argc, char **argv)
{
    uint64_t t0, t;
    int i, base;
    char ring[LJ_LOG_RING_LINE];
    (void)argc; (void)argv;

    lj_log_set_sink(lj_capture, NULL);
    t0 = lj_clock_us();
    for (i = 0; i < 100000; i++) { lj_lock(); lj_unlock(); }
    t = lj_clock_us() - t0;
    printf("test_lock: 100000 lock/unlock pairs in %u us\n", (unsigned)t);
    CHECK(s_hold_lines <= 2);                                  /* a pair preempted on a busy CI runner can hold > 2 ms (W3 CI, Debug ctest -j) */
    base = s_hold_lines;

    lj_lock();
    t0 = lj_clock_us();
    while (lj_clock_us() - t0 < 3000) { }                     /* hold 3 ms, over the 2 ms rule */
    lj_unlock();
#if defined(LJ_DEBUG) || !defined(NDEBUG)
    CHECK(s_hold_lines == base + 1);
    CHECK(lj_log_ring(0, ring, sizeof ring) == 1 && strncmp(ring, "lock: held", 10) == 0);
#else
    CHECK(s_hold_lines == base);
    printf("test_lock: hold measurement compiled out (NDEBUG without LJ_DEBUG)\n");
    (void)ring;
#endif
    lj_log_set_sink(NULL, NULL);
    printf("test_lock: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
