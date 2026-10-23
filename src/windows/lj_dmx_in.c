/* lj_dmx_in.c - W4 contract stub (docs/w4-contracts.md): the hw-dmxin track replaces these bodies. Until then every id of
 * the family logs the census line "has no behaviour file yet" (ME-131) and opens nothing. */
#include "windows/lj_dmx_in.h"
#include "core/lj_log.h"

lj_window *lj_dmx_in_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_dmx_in_close(enum lj_win id) { (void)id; }
lj_window *lj_dmx_in_get(enum lj_win id) { (void)id; return NULL; }
void       lj_dmx_in_refresh(enum lj_win id) { (void)id; }
void       lj_dmx_in_bind_fns(void) { }
