/* lj_log.h - the log sink: the one global in the program (design 2.0, 2.1).
 * Default sink writes to stderr. One line per call; the Debug window installs
 * its own sink in a later wave. lj_fatal() (lj_types.h) is implemented beside
 * this in lj_log.c. */
#ifndef LJ_CORE_LOG_H
#define LJ_CORE_LOG_H

#include <stdint.h>
#include "core/lj_types.h"

typedef void (*lj_log_sink)(void *ud, const char *line);   /* line has no trailing newline; CP1252 */

void lj_log(const char *fmt, ...) LJ_PRINTF(1, 2);        /* formats, then hands the line to the sink; thread-safe with respect to lj_lock() holders */
void lj_log_set_sink(lj_log_sink fn, void *ud);            /* NULL fn restores stderr */
void lj_log_overrun(uint64_t late_us);                     /* the engine thread's "tick ran late" line (design 3.1) */

/* The ring of the last LJ_LOG_RING lines, kept beside the sink for the Debug window. */
enum { LJ_LOG_RING = 256, LJ_LOG_RING_LINE = 256 };
int  lj_log_ring(int i, char *dst, size_t cap);            /* i = 0 newest, 1 next older ...; 1 = line produced, 0 = none */

#endif /* LJ_CORE_LOG_H */
