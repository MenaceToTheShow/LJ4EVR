/* lj_da2532.c - W4 contract stub (docs/w4-contracts.md): the da2532-fingers track replaces these bodies. Until then every id of
 * the family logs the census line "has no behaviour file yet" (ME-131) and opens nothing. */
#include "windows/lj_da2532.h"
#include "core/lj_log.h"

lj_window *lj_da2532_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_da2532_close(enum lj_win id) { (void)id; }
lj_window *lj_da2532_get(enum lj_win id) { (void)id; return NULL; }
void       lj_da2532_refresh(enum lj_win id) { (void)id; }
void       lj_da2532_bind_fns(void) { }
