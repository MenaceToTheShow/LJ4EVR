/* lj_clock.h - monotonic microsecond clock (design 2.1).
 * QueryPerformanceCounter / mach_absolute_time / clock_gettime(CLOCK_MONOTONIC);
 * lj_clock.c is on the OS-header allowlist of tools/test_layering.py (windows.h, time.h). */
#ifndef LJ_CORE_CLOCK_H
#define LJ_CORE_CLOCK_H

#include <stdint.h>

uint64_t lj_clock_us(void);                          /* monotonic microseconds since an unspecified epoch; never goes backwards */

#endif /* LJ_CORE_CLOCK_H */
