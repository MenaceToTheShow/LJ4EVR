/* lj_lock.c - THE show mutex (design 2.1, 3.1): SRWLOCK on Windows (static init,
 * unlike CRITICAL_SECTION), pthread mutex elsewhere; not recursive. With LJ_DEBUG
 * or without NDEBUG, lj_unlock measures the hold and logs anything over 2 ms
 * (the 3.1 lock rule; test_lock_hold asserts it). */
#include "core/lj_lock.h"
#include "core/lj_clock.h"
#include "core/lj_log.h"

#if defined(LJ_DEBUG) || !defined(NDEBUG)
#define LJ_LOCK_MEASURE 1
#define LJ_LOCK_HOLD_MAX_US 2000
static uint64_t s_held_since;
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static SRWLOCK s_lock = SRWLOCK_INIT;
#define LJ_LOCK_ACQUIRE() AcquireSRWLockExclusive(&s_lock)
#define LJ_LOCK_RELEASE() ReleaseSRWLockExclusive(&s_lock)
#else
#include <pthread.h>
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
#define LJ_LOCK_ACQUIRE() do { if (pthread_mutex_lock(&s_lock) != 0) lj_fatal("lj_lock: pthread_mutex_lock failed"); } while (0)
#define LJ_LOCK_RELEASE() do { if (pthread_mutex_unlock(&s_lock) != 0) lj_fatal("lj_unlock: pthread_mutex_unlock failed"); } while (0)
#endif

void lj_lock(void)
{
    LJ_LOCK_ACQUIRE();
#ifdef LJ_LOCK_MEASURE
    s_held_since = lj_clock_us();
#endif
}

void lj_unlock(void)
{
#ifdef LJ_LOCK_MEASURE
    uint64_t held = lj_clock_us() - s_held_since;
    if (held > LJ_LOCK_HOLD_MAX_US) lj_log("lock: held %u us (limit %u, design 3.1)", (unsigned)held, (unsigned)LJ_LOCK_HOLD_MAX_US);
#endif
    LJ_LOCK_RELEASE();
}
