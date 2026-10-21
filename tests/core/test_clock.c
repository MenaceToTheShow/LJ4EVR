/* test_clock.c - lj_clock_us never goes backwards and advances at wall-clock rate. */
#include <stdio.h>
#include "core/lj_clock.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_clock_monotonic(int argc, char **argv);
int lj_test_clock_monotonic(int argc, char **argv)
{
    uint64_t prev = lj_clock_us(), t0 = prev, t, d;
    int i, back = 0;
    (void)argc; (void)argv;
    for (i = 0; i < 1000000; i++) {
        t = lj_clock_us();
        if (t < prev) back++;
        prev = t;
    }
    CHECK(back == 0);
    t0 = lj_clock_us();
    while (lj_clock_us() - t0 < 5000) { }
    d = lj_clock_us() - t0;
    CHECK(d >= 5000 && d < 1000000);                 /* 5 ms busy wait reads as 5..1000 ms */
    printf("test_clock_monotonic: 1000000 reads, %d backwards, 5 ms wait read %u us, %d failure(s)\n", back, (unsigned)d, fails);
    return fails ? 1 : 0;
}
