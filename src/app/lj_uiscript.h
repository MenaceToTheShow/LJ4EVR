/* lj_uiscript.h - the UI script driver (docs/ui-script.md): `lj4evr --script
 * <file>` executes one line per event-loop pass on the UI thread: open/close a
 * window, click/rclick/dblclick/drag a widget by <Form>.<widget>, type text,
 * press a key, wait, screenshot, assert captions/text/enabled/frame values/files,
 * quit. A failure prints "SCRIPT FAIL line n: <reason>" to stderr and the app
 * exits 1; "quit" exits 0. Test affordance, ponytail-sized. SDL-free: it only
 * synthesises lj_ui_events into lj_window_event, so tests/ui/test_uiscript.c
 * runs the same file headless. Window names resolve through lj_win_find() (DFM
 * class or docs/ui-script.md id), `main` and the control kinds; widgets through
 * lj_window_find(). */
#ifndef LJ_APP_UISCRIPT_H
#define LJ_APP_UISCRIPT_H

#include <stddef.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

/* What the host (the app, or a test) supplies: a window resolver that may wrap
 * lj_uiscript_resolve_default, and the app's last lj_engine_view copy for
 * assert_view / assert_pps (only lj_app_service_pending may call lj_engine_view,
 * design 2.3 rule 3). NULL members = default resolver / "no view". */
typedef struct { void *ud; lj_window *(*resolve)(void *ud, const char *form); const lj_engine_status *(*view)(void *ud); } lj_uiscript_host;
void       lj_uiscript_set_host(const lj_uiscript_host *host);   /* NULL = defaults; the struct is copied */
lj_window *lj_uiscript_resolve_default(const char *form);        /* "main" -> lj_desktop_window, a control kind -> lj_ctl_window_get, else lj_win_find */

int  lj_uiscript_load(const char *path);     /* reads and parses the whole file; 0 / -1 + "SCRIPT FAIL line n: <reason>" (n = 0: cannot read) */
int  lj_uiscript_step(void);                 /* called by the app's idle pass: executes the next line (a `wait` spans passes); 1 = more to do, 0 = finished (quit or EOF), -1 = failed (already printed) */
int  lj_uiscript_active(void);               /* a script is loaded and not finished */
int  lj_uiscript_line(void);                 /* the current 1-based line, for log lines */
void lj_uiscript_fail(const char *reason);   /* "SCRIPT FAIL line n: reason" and mark finished with -1; windows may call it from an assert hook */

#endif /* LJ_APP_UISCRIPT_H */
