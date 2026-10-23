/* lj_media_form.h - W5 window family MEDIA (docs/w5-contracts.md seam 9), owned by the media track. lj_windows.c dispatches
 * its ids here; the track replaces the stub bodies of lj_media_form.c and adds what it needs below the contract block.
 *   LJ_WIN_MEDIA_PLAYER  TLJMediaForm (LJMediaPlayer.exe): cue list ListPop LJ Mediaplayer, the transport [05 s8.1, design 8.5] */
#ifndef LJ_WINDOWS_MEDIA_FORM_H
#define LJ_WINDOWS_MEDIA_FORM_H

#include "windows/lj_windows.h"

lj_window *lj_media_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_media_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_media_form_get(enum lj_win id);       /* NULL when closed */
void       lj_media_form_refresh(enum lj_win id);   /* repaint from the module state / engine view copy; no-op when closed */
void       lj_media_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- media track (below this line) ---- */

#endif /* LJ_WINDOWS_MEDIA_FORM_H */
