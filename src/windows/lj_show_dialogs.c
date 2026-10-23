/* lj_show_dialogs.c - W4 contract stub (docs/w4-contracts.md): the dlg-show track replaces these bodies. Until then every id of
 * the family logs the census line "has no behaviour file yet" (ME-131) and opens nothing. */
#include "windows/lj_show_dialogs.h"
#include "core/lj_log.h"

lj_window *lj_show_dlg_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_show_dlg_close(enum lj_win id) { (void)id; }
lj_window *lj_show_dlg_get(enum lj_win id) { (void)id; return NULL; }
void       lj_show_dlg_refresh(enum lj_win id) { (void)id; }
void       lj_show_dlg_bind_fns(void) { }
