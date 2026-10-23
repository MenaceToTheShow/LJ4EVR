/* lj_audio_form.h - W5 window family AUDIO (docs/w5-contracts.md seam 9), owned by the audio track. lj_windows.c dispatches
 * its ids here; the track replaces the stub bodies of lj_audio_form.c and adds what it needs below the contract block.
 *   LJ_WIN_AUDIO_ANALYZER  TLJSoundApp (LJSound.exe): &Audio > Audio Analyzer (M_AudioClick); M_AudioOptionsClick opens the Hardware Setup tsAudio page [05 s9.1] */
#ifndef LJ_WINDOWS_AUDIO_FORM_H
#define LJ_WINDOWS_AUDIO_FORM_H

#include "windows/lj_windows.h"

lj_window *lj_audio_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_audio_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_audio_form_get(enum lj_win id);       /* NULL when closed */
void       lj_audio_form_refresh(enum lj_win id);   /* repaint from the module state / engine view copy; no-op when closed */
void       lj_audio_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- audio track (below this line) ---- */

#endif /* LJ_WINDOWS_AUDIO_FORM_H */
