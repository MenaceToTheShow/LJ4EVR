/* lj_remote_form.c - the REMOTE family (windows/lj_remote_form.h). W5 CONTRACT STUB: the remote track replaces every body
 * (docs/w5-contracts.md seam 9). Until then its ids log "has no behaviour file yet" and open nothing. */
#include "windows/lj_remote_form.h"
#include "core/lj_log.h"

lj_window *lj_remote_form_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_remote_form_close(enum lj_win id) { (void)id; }
lj_window *lj_remote_form_get(enum lj_win id) { (void)id; return NULL; }
void       lj_remote_form_refresh(enum lj_win id) { (void)id; }
void       lj_remote_form_bind_fns(void) {}
