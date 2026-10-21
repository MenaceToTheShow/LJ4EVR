/* lj_w_groupbox.h - TGroupBox; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_GROUPBOX_H
#define LJ_UI_WIDGETS_W_GROUPBOX_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_groupbox_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_groupbox_paint(lj_widget *w, lj_surface *s);   /* shared with TRadioGroup */

#endif /* LJ_UI_WIDGETS_W_GROUPBOX_H */
