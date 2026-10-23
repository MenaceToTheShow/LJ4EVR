/* lj_w_bitbtn.h - see lj_widgets_basic.h. */
#ifndef LJ_UI_WIDGETS_W_BITBTN_H
#define LJ_UI_WIDGETS_W_BITBTN_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

lj_widget *lj_w_bitbtn_create(lj_widget *parent, const lj_ctl_desc *d);
const char *lj_w_bitbtn_kind_glyph(const lj_widget *w);   /* ME-136: the art stem of a TBitBtn's Kind when the DFM gives no Glyph; NULL otherwise */

#endif /* LJ_UI_WIDGETS_W_BITBTN_H */
