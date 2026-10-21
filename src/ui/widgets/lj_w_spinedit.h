/* lj_w_spinedit.h - TSpinEdit (RX/Delphi samples): a sunken numeric edit with a
 * 15-px up/down pair inside its right end; Value in MinValue..MaxValue. */
#ifndef LJ_UI_WIDGETS_W_SPINEDIT_H
#define LJ_UI_WIDGETS_W_SPINEDIT_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_W_SPIN_BTN_W 15

typedef struct { lj_widget base; int value, min_value, max_value, max_length; unsigned pressed:2; } lj_w_spinedit;

lj_widget *lj_w_spinedit_create(lj_widget *parent, const lj_ctl_desc *d);
int        lj_w_spinedit_value(const lj_widget *w);
int        lj_w_spinedit_set_value(lj_widget *w, int v);                   /* clamps; no event */

#endif /* LJ_UI_WIDGETS_W_SPINEDIT_H */
