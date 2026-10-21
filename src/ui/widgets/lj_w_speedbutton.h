/* lj_w_speedbutton.h - TSpeedButton; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_SPEEDBUTTON_H
#define LJ_UI_WIDGETS_W_SPEEDBUTTON_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_speedbutton_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_speedbutton_click(lj_widget *w);                 /* GroupIndex/AllowAllUp latch logic, then OnClick */
int        lj_w_speedbutton_down(const lj_widget *w);
void       lj_w_speedbutton_set_down(lj_widget *w, int down);    /* TSpeedButton.SetDown: no-op without GroupIndex or against AllowAllUp = False */

#endif /* LJ_UI_WIDGETS_W_SPEEDBUTTON_H */
