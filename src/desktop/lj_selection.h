/* lj_selection.h - fixture selection state (design 7.8, [01 s3.2-3.3], F12): master /
 * slaves, Inclusive-Exclusive, solo, serial. Public API: lj_sel_* in lj_desktop.h;
 * this header adds what the icon area and popups need. Selection.Dat is W2. */
#ifndef LJ_DESKTOP_SELECTION_H
#define LJ_DESKTOP_SELECTION_H

#include "desktop/lj_desktop.h"

int  lj_sel_same_type(int a, int b);      /* same kind + profile id (or .GS2 file) [01 s3.6 "of this type"] */
void lj_sel_reset(void);                  /* after a Config.dat reload: nothing selected, Inclusive, solo off */

#endif /* LJ_DESKTOP_SELECTION_H */
