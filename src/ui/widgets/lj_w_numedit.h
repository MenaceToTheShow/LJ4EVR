/* lj_w_numedit.h - TNumEdit; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_NUMEDIT_H
#define LJ_UI_WIDGETS_W_NUMEDIT_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_numedit_create(lj_widget *parent, const lj_ctl_desc *d);
int        lj_w_numedit_value(const lj_widget *w);               /* the clamped Value; 0 for a non-numedit */
int        lj_w_numedit_set_value(lj_widget *w, int v);          /* clamps to Min..Max, rewrites the text, fires OnChange/OnValueChange; -1 for a non-numedit */

#endif /* LJ_UI_WIDGETS_W_NUMEDIT_H */
