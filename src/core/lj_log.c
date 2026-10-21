/* lj_log.c - the log sink, the one global in the program (design 2.0, 2.1), the
 * ring of the last LJ_LOG_RING lines for the Debug window, and lj_fatal. */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"

#define LJ_LOG_LINE 512

static lj_log_sink s_sink;
static void       *s_sink_ud;
static char        s_ring[LJ_LOG_RING][LJ_LOG_RING_LINE];
static unsigned    s_ring_n;      /* lines ever logged; the ring holds the last LJ_LOG_RING of them */
/* ponytail: the ring is unguarded; two threads logging in the same microsecond may
 * tear one diagnostic line. Upgrade: a second mutex in lj_lock.c if it ever shows. */

static void lj_log_stderr(void *ud, const char *line)
{
    (void)ud;
    fputs(line, stderr);
    fputc('\n', stderr);
}

static void lj_log_line(const char *line)
{
    lj_str_copy(s_ring[s_ring_n % LJ_LOG_RING], LJ_LOG_RING_LINE, line);
    s_ring_n++;
    (s_sink ? s_sink : lj_log_stderr)(s_sink_ud, line);
}

void lj_log(const char *fmt, ...)
{
    char buf[LJ_LOG_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    lj_log_line(buf);
}

void lj_log_set_sink(lj_log_sink fn, void *ud)
{
    s_sink = fn;
    s_sink_ud = ud;
}

void lj_log_overrun(uint64_t late_us)
{
    lj_log("engine: tick ran %" PRIu64 " us late", late_us);
}

int lj_log_ring(int i, char *dst, size_t cap)
{
    if (i < 0 || (unsigned)i >= s_ring_n || i >= LJ_LOG_RING) return 0;
    lj_str_copy(dst, cap, s_ring[(s_ring_n - 1 - (unsigned)i) % LJ_LOG_RING]);
    return 1;
}

void lj_fatal(const char *fmt, ...)
{
    char buf[LJ_LOG_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    lj_log("fatal: %s", buf);
    abort();
}
