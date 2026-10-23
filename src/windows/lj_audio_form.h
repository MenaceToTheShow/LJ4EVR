/* lj_audio_form.h - W5 window family AUDIO (docs/w5-contracts.md seam 9), owned by the audio track. lj_windows.c dispatches
 * its ids here; the track replaces the stub bodies of lj_audio_form.c and adds what it needs below the contract block.
 *   LJ_WIN_AUDIO_ANALYZER  TLJSoundApp (LJSound.exe): &Audio > Audio Analyzer (M_AudioClick); M_AudioOptionsClick opens the Hardware Setup tsAudio page [05 s9.1] */
#ifndef LJ_WINDOWS_AUDIO_FORM_H
#define LJ_WINDOWS_AUDIO_FORM_H

#include <stdint.h>
#include "ui/forms/lj_form.h"
#include "windows/lj_windows.h"

lj_window *lj_audio_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_audio_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_audio_form_get(enum lj_win id);       /* NULL when closed */
void       lj_audio_form_refresh(enum lj_win id);   /* repaint from the module state / engine view copy; no-op when closed */
void       lj_audio_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- audio track (below this line) ---- */
/* The Analyzer window is TLJSoundApp of LJSound.exe, transcribed from dfm2c.py run on that exe's TPF0 resource
 * (captions, geometry and menu tree verbatim, D2 / D13; no Martin art). The Hardware Setup tsAudio page (RGAudio and a
 * code-built device combo) is this family's: lj_hardware_setup.c calls the two functions below in its W5 audio block. */
extern const lj_form_desc lj_form_TLJSOUNDAPP;
void lj_audio_form_hs_build(lj_window *hs);   /* tsAudio live: RGAudio from ctx->audio, the device list (default first), CD / Digital Audio rows greyed */
void lj_audio_form_hs_apply(void);            /* OK: Enabled / device into ctx->audio; LJ_ACT_IO_APPLY LJ_IO_AUDIO when either changed */
void lj_audio_form_hs_closed(void);           /* the Hardware Setup window is gone */
int  lj_audio_form_trig_lit(uint64_t now_us); /* 1 while the last analyzer trig is under 100 ms old (the status-bar Trig panels) */

#endif /* LJ_WINDOWS_AUDIO_FORM_H */
