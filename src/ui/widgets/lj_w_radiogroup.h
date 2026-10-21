/* lj_w_radiogroup.h - TRadioGroup; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_RADIOGROUP_H
#define LJ_UI_WIDGETS_W_RADIOGROUP_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_radiogroup_create(lj_widget *parent, const lj_ctl_desc *d);   /* builds one TRadioButton child per Items string */
int        lj_w_radiogroup_index(const lj_widget *w);                          /* ItemIndex, -1 none */
int        lj_w_radiogroup_set_index(lj_widget *w, int i);                     /* checks that item (fires OnClick as the VCL does); -1 out of range */
void       lj_w_radiogroup_child_checked(lj_widget *group, lj_widget *child);  /* called by lj_w_radiobutton_click */

#endif /* LJ_UI_WIDGETS_W_RADIOGROUP_H */
