/* lj_w_checkbox.h - TCheckBox; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_CHECKBOX_H
#define LJ_UI_WIDGETS_W_CHECKBOX_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_checkbox_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_checkbox_click(lj_widget *w);                     /* toggles, then OnClick */
int        lj_w_checkbox_checked(const lj_widget *w);
void       lj_w_checkbox_set_checked(lj_widget *w, int on);       /* TCheckBox.SetChecked: fires OnClick when it changes, as the VCL does */

#endif /* LJ_UI_WIDGETS_W_CHECKBOX_H */
