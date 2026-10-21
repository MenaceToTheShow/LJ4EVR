/* lj_w_panel.h - TPanel; see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_PANEL_H
#define LJ_UI_WIDGETS_W_PANEL_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_panel_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_panel_paint_frame(lj_widget *w, lj_surface *s, lj_rect *inner);   /* face + bevels from the desc; inner = the rect left for content (absolute) */

#endif /* LJ_UI_WIDGETS_W_PANEL_H */
