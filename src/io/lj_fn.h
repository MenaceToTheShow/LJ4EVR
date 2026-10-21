/* lj_fn.h - running a hotkey function (design 2.5, 7.9). UI thread only:
 * keyboard, the cue list "Hotkey Command", 2532 and Fingers call lj_fn_run()
 * directly; MIDI reaches it through the event the UI loop dispatches (2.3 rule
 * 5). Windows and the desktop register one handler per function at startup
 * (lj_fn_bind); an unbound function logs once and does nothing. The binding
 * table is file-static in lj_fn.c: the design declares lj_fn_run without a
 * context, and there is one UI thread. */
#ifndef LJ_IO_FN_H
#define LJ_IO_FN_H

#include <stdint.h>
#include "io/lj_actions.h"

typedef void (*lj_fn_handler)(enum lj_fn fn, int edge /* 1 press, 0 release */, uint32_t prog_num, void *ud);

void lj_fn_run(enum lj_fn fn, int edge);                                 /* UI thread only; edge 0 matters only for flash functions (LJ_FNF_FLASH) */
void lj_fn_run_prog(enum lj_fn fn, uint32_t prog_num, int edge);         /* the LJ_FN_PROG_* rows with their bound number */
int  lj_fn_bind(enum lj_fn fn, lj_fn_handler handler, void *ud);         /* replaces any previous binding; -1 for LJ_FN_NONE / out of range */
int  lj_fn_bound(enum lj_fn fn);                                         /* 1 when a handler is registered (Debug --selftest lists the unbound rows) */
void lj_fn_unbind_all(void);                                             /* shutdown */

#endif /* LJ_IO_FN_H */
