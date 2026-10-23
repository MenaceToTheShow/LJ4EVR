/* lj_lightcorder.c - W4 contract stub (docs/w4-contracts.md): the lightcorder-debug track replaces these bodies. Until then every id of
 * the family logs the census line "has no behaviour file yet" (ME-131) and opens nothing. */
#include "windows/lj_lightcorder.h"
#include "core/lj_log.h"

lj_window *lj_lightcorder_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_lightcorder_close(enum lj_win id) { (void)id; }
lj_window *lj_lightcorder_get(enum lj_win id) { (void)id; return NULL; }
void       lj_lightcorder_refresh(enum lj_win id) { (void)id; }
void       lj_lightcorder_bind_fns(void) { }
void       lj_lightcorder_step(uint64_t now_us) { (void)now_us; }
