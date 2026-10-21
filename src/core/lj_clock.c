/* lj_clock.c - monotonic microsecond clock (design 2.1). QueryPerformanceCounter on
 * Windows, clock_gettime(CLOCK_MONOTONIC) elsewhere (macOS 10.12+, so also our
 * 12.0 floor; no mach headers needed). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L   /* clock_gettime under -std=c11 */
#endif
#include "core/lj_clock.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

uint64_t lj_clock_us(void)
{
    static LARGE_INTEGER freq;              /* constant for the process, fetched once */
    LARGE_INTEGER c;
    uint64_t f, n;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    f = (uint64_t)freq.QuadPart;
    n = (uint64_t)c.QuadPart;
    return (n / f) * 1000000u + (n % f) * 1000000u / f;   /* split so n * 1e6 cannot overflow */
}
#else
#include <time.h>

uint64_t lj_clock_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
#endif
