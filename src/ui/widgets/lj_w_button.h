/* lj_w_button.h - TButton (and TBitBtn, which shares the struct and vt); see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_BUTTON_H
#define LJ_UI_WIDGETS_W_BUTTON_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_button_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_button_paint(lj_widget *w, lj_surface *s);           /* raised/pressed face, caption, default frame, focus rect */
int        lj_w_button_mouse_ev(lj_widget *w, const lj_mouse_ev *m);
int        lj_w_button_key(lj_widget *w, const lj_key_ev *k);        /* Space clicks */
int        lj_w_button_is_default(const lj_widget *w);               /* Default = True, or Kind bkOK/bkYes */

#endif /* LJ_UI_WIDGETS_W_BUTTON_H */
