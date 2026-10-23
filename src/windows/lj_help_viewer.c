/* lj_help_viewer.c - W4 contract stub (docs/w4-contracts.md): the docs-release track replaces these bodies. Until then every id of
 * the family logs the census line "has no behaviour file yet" (ME-131) and opens nothing. */
#include "windows/lj_help_viewer.h"
#include "core/lj_log.h"

lj_window *lj_help_viewer_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_help_viewer_close(enum lj_win id) { (void)id; }
lj_window *lj_help_viewer_get(enum lj_win id) { (void)id; return NULL; }
void       lj_help_viewer_refresh(enum lj_win id) { (void)id; }
void       lj_help_viewer_bind_fns(void) { }
int        lj_help_viewer_topic(const char *topic) { (void)topic; return lj_help_viewer_open(LJ_WIN_HELP) ? 0 : -1; }
