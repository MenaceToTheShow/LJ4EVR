/* lj_main_window.h - the code-built main window (design 7.7, [07 s4], F1/F2/F5):
 * the generated TLJMainForm menu made live through ONE table (ME-29): top-level
 * visibility per F5, dropped leaves greyed with the hint "Not available in
 * LJ4EVR", View > Toolbars > Offline Toolbar removed (D12, docs/fidelity.md),
 * the two desktop popups verbatim from [01 s3.5-3.6]. Every DFM handler name
 * is bound (ME-16). lj_desktop_create() (lj_desktop.h) is the entry; the W0
 * names below stay for lj_app.c / lj4evr_render until the app track moves. */
#ifndef LJ_DESKTOP_MAIN_WINDOW_H
#define LJ_DESKTOP_MAIN_WINDOW_H

#include <stdint.h>
#include "ui/lj_window.h"

lj_window *lj_main_window_create(const lj_binding *bindings);   /* W0 name: = lj_desktop_create(); only a caller's "FormClose" row is honoured (the W0 lj_app noop rows are replaced by this file, ME-29) */
void       lj_main_window_destroy(lj_window *win);               /* W0 name: = lj_desktop_destroy() */

lj_window *lj_main_window_build(const lj_binding *extra);        /* the bare window: desc (TLJMainForm menu + popups) + bindings; extra = caller rows tried first, NULL-terminated or NULL */
void       lj_main_window_free(lj_window *win);                  /* destroys the window and its heap desc */
void       lj_main_menu_apply(lj_window *win);                   /* the ME-29 table onto the live menu: visibility, inert leaves, check marks */
void       lj_main_menu_sync(lj_window *win);                    /* check marks from the toolbar / hotkey / option state (parent-item OnClicks call it) */

/* The table, exported for tests/ui/test_desktop.c. */
typedef struct { const char *name; uint8_t visible; const char *why; } lj_main_top_rule;   /* visible: 0 hidden, 1 shown, 2 = shown when a generic fixture is configured, 3 = while the Audio Analyzer is enabled (ME-181) */
typedef struct { const char *name; const char *hint; } lj_main_inert_leaf;               /* greyed, hint LJ_NA_HINT */
const lj_main_top_rule   *lj_main_menu_top_table(int *n);
const lj_main_inert_leaf *lj_main_menu_inert_table(int *n);
const char *lj_main_menu_removed_item(void);                     /* "Offlinebar1" (D12) */
const lj_binding *lj_main_window_bindings(void);                 /* the full table, NULL-terminated */
extern const lj_menu_desc lj_main_popup_desktop, lj_main_popup_icon;   /* [01 s3.5], [01 s3.6] */

#endif /* LJ_DESKTOP_MAIN_WINDOW_H */
