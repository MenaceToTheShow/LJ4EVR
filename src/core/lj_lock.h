/* lj_lock.h - THE show mutex (design 2.1, 3.1).
 * Held by the engine for one tick and by the UI for one model mutation; never
 * across file I/O, dialogs or drawing. Statically initialised (SRWLOCK /
 * PTHREAD_MUTEX_INITIALIZER), so there is no init call. Not recursive.
 * lj_lock.c is on the OS-header allowlist (windows.h, pthread.h). */
#ifndef LJ_CORE_LOCK_H
#define LJ_CORE_LOCK_H

void lj_lock(void);
void lj_unlock(void);

#endif /* LJ_CORE_LOCK_H */
