/* lj_fn.c - the function dispatcher of lj_fn.h: one handler slot per lj_fn row,
 * UI thread only. The one file-static table in io/ (see the header). */
#include <string.h>
#include "io/lj_fn.h"
#include "core/lj_log.h"

static struct { lj_fn_handler fn; void *ud; } lj_fn_slots[LJ_FN_COUNT];
static uint8_t lj_fn_warned[LJ_FN_COUNT];

void lj_fn_run_prog(enum lj_fn fn, uint32_t prog_num, int edge)
{
    if ((int)fn <= 0 || fn >= LJ_FN_COUNT) return;
    if (edge == 0 && !lj_fn_is_flash(fn)) return;            /* release matters only for flash functions */
    if (!lj_fn_slots[fn].fn) {
        if (!lj_fn_warned[fn]) { lj_fn_warned[fn] = 1; lj_log("fn: no binding for %s", lj_fn_table[fn].name); }
        return;
    }
    lj_fn_slots[fn].fn(fn, edge, prog_num, lj_fn_slots[fn].ud);
}

void lj_fn_run(enum lj_fn fn, int edge) { lj_fn_run_prog(fn, 0, edge); }

int lj_fn_bind(enum lj_fn fn, lj_fn_handler handler, void *ud)
{
    if ((int)fn <= 0 || fn >= LJ_FN_COUNT) return -1;
    lj_fn_slots[fn].fn = handler; lj_fn_slots[fn].ud = ud;
    return 0;
}

int lj_fn_bound(enum lj_fn fn) { return ((int)fn > 0 && fn < LJ_FN_COUNT) ? lj_fn_slots[fn].fn != NULL : 0; }

void lj_fn_unbind_all(void) { memset(lj_fn_slots, 0, sizeof lj_fn_slots); memset(lj_fn_warned, 0, sizeof lj_fn_warned); }
