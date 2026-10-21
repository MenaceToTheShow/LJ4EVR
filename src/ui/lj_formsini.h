/* lj_formsini.h - forms.ini / Main.Ini window-position persistence (design 7.10).
 * Persisted: forms whose generated Position is poDesigned or poDefaultPosOnly, or
 * whose class maps to a known LJ2 section. Everything else (the 138
 * poOwnerFormCenter/poMainFormCenter and 7 poScreenCenter forms) is centred on
 * its owner at every open and never saved. Keys are LJ2's: Left, Top, and
 * Width, Height for sizeable forms. Code-built windows (Intensity, Movement ...)
 * use the section-level calls with their own section names. */
#ifndef LJ_UI_FORMSINI_H
#define LJ_UI_FORMSINI_H

#include <stddef.h>
#include "core/lj_ini.h"
#include "ui/forms/lj_form.h"

#define LJ_FORMSINI_FORMS 0
#define LJ_FORMSINI_MAIN  1

int  lj_formsini_section(const lj_form_desc *d, char *out, size_t cap);   /* LJ_FORMSINI_FORMS / LJ_FORMSINI_MAIN with the section in out; -1 = never persisted (centre on the owner) */
int  lj_formsini_restore(lj_ini *ini, const char *section, int *x, int *y, int *w, int *h);   /* 0 when Left/Top are present (w/h only overwritten when both are present and non-NULL); -1 = nothing saved, use the design Left/Top */
void lj_formsini_save(lj_ini *ini, const char *section, int sizeable, int x, int y, int w, int h);   /* Width/Height written only when sizeable */
void lj_formsini_center(int ox, int oy, int ow, int oh, int w, int h, int *x, int *y);          /* the VCL centre: owner centre minus half the size */
int  lj_formsini_sizeable(const lj_form_desc *d);                                               /* bsSizeable / bsSizeToolWin */

#endif /* LJ_UI_FORMSINI_H */
