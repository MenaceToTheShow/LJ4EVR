/* lj_audio_form.c - the AUDIO family (windows/lj_audio_form.h). W5 CONTRACT STUB: the audio track replaces every body
 * (docs/w5-contracts.md seam 9). Until then its ids log "has no behaviour file yet" and open nothing. */
#include "windows/lj_audio_form.h"
#include "core/lj_log.h"

lj_window *lj_audio_form_open(enum lj_win id) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
void       lj_audio_form_close(enum lj_win id) { (void)id; }
lj_window *lj_audio_form_get(enum lj_win id) { (void)id; return NULL; }
void       lj_audio_form_refresh(enum lj_win id) { (void)id; }
void       lj_audio_form_bind_fns(void) {}
