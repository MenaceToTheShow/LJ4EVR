/* lj_types.h - fixed-width types, the engine-facing limits, lj_fatal, attribute macros.
 * Design 2.0/2.1. Every header under src/ may include this one. */
#ifndef LJ_CORE_TYPES_H
#define LJ_CORE_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Limits the engine, io and gfx layers need without model/lj_model.h. The model's
 * limits enum (design 4) includes this header and adds the rest; it must not
 * redefine these six. */
enum {
    LJ_LINKS     = 4,      /* DMX links (universes) */
    LJ_LINK_CH   = 512,    /* channels per link */
    LJ_CHANS     = 2048,   /* LJ_LINKS * LJ_LINK_CH, the address code space [18 §3] */
    LJ_FIXTURES  = 100,
    LJ_CUE_SLOTS = 12,
    LJ_BG_SLOTS  = 5
};

/* printf-format checking: LJ_PRINTF(fmt_index, first_vararg_index), 1-based. */
#if defined(__GNUC__) || defined(__clang__)
#define LJ_PRINTF(f, a) __attribute__((format(printf, f, a)))
#else
#define LJ_PRINTF(f, a)
#endif

/* ME-29 / ME-173: the one hint on greyed chrome (design 1, the sacrifice list); ours, not in the exe pool. Greyed and
 * un-greyed only through lj_widget_set_enabled (ui/lj_widget.h) or a static desc row (.enabled = LJ_NO, .hint = it). */
#define LJ_NA_HINT "Not available in LJ4EVR"

#define LJ_COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Logs one line through the log sink, then aborts. The only exit path for a
 * failed malloc outside engine/ (design 2.0). Implemented in core/lj_log.c. */
_Noreturn void lj_fatal(const char *fmt, ...) LJ_PRINTF(1, 2);

#endif /* LJ_CORE_TYPES_H */
