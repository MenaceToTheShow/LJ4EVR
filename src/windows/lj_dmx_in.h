/* lj_dmx_in.h - W4 window family DMXIN (docs/w4-contracts.md section 3), owned by the hw-dmxin track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_dmx_in.c and adds what it needs below the contract block.
 *   LJ_WIN_DMX_IN  TDmxInForm: C-L toolbar DMX In (LJ_ACT_DMX_IN), Global Patch DMX-in source [04 s15, 05 s3]
 *   LJ_WIN_DMXIN_TRANSLATION  TDMXInValueTranslationForm: Dmx In translation tables [05 s3]
 *   LJ_WIN_TRANSLATION_EDIT  TTranslationEditForm: DMX-In translation Edit [05 s3]
 *   LJ_WIN_VALUE_TABLE_ERROR  TValueTableImportErrorForm: DMX-In translation import error [05 s3]
 *   LJ_WIN_EXPORT_TABLE  TExportTableForm: DMX-In translation Export Value tables (.vtr) [05 s3]
 */
#ifndef LJ_WINDOWS_DMX_IN_H
#define LJ_WINDOWS_DMX_IN_H

#include "windows/lj_windows.h"

lj_window *lj_dmx_in_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_dmx_in_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_dmx_in_get(enum lj_win id);       /* NULL when closed */
void       lj_dmx_in_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_dmx_in_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- hw-dmxin track (W4-DMX, W4-HW-0, I7: window W4, input W5) ---- */
#define LJ_DMX_IN_NA "Not available in LJ4EVR"     /* design 1 / ME-29 hint; ours, not in the exe pool */
void lj_dmx_in_grey(lj_widget *root, const char *const *keep, int nkeep);   /* every input control under root (keep = names left live) disabled, hint LJ_DMX_IN_NA where the DFM gives none */

#endif /* LJ_WINDOWS_DMX_IN_H */
