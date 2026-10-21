/* lj_w_radiobutton.h - TRadioButton; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_RADIOBUTTON_H
#define LJ_UI_WIDGETS_W_RADIOBUTTON_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_radiobutton_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_radiobutton_click(lj_widget *w);                  /* checks it, unchecks its sibling radios, tells a TRadioGroup parent, then OnClick */
int        lj_w_radiobutton_checked(const lj_widget *w);

#endif /* LJ_UI_WIDGETS_W_RADIOBUTTON_H */
