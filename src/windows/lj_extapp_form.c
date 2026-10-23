/* lj_extapp_form.c - the EXTAPP family (windows/lj_extapp_form.h). W5 CONTRACT STUB: the extapp track replaces every body
 * (docs/w5-contracts.md seam 9). Until then its ids log "has no behaviour file yet" and open nothing. */
#include "windows/lj_extapp_form.h"
#include "core/lj_log.h"

lj_window *lj_extapp_form_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_extapp_form_close(enum lj_win id) { (void)id; }
lj_window *lj_extapp_form_get(enum lj_win id) { (void)id; return NULL; }
void       lj_extapp_form_refresh(enum lj_win id) { (void)id; }
void       lj_extapp_form_bind_fns(void) {}
void       lj_extapp_form_target(int fixture) { (void)fixture; }
