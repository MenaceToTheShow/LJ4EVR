/* lj_w_slotbtn.h - a Statics 2 slot button (note 14 §B.3; runtime-built in LJ2, the
 * look taken from the old control's staticcontrol.gif): a raised button with the
 * entry name left-aligned, drawn pressed while the slot is active. Paint only:
 * the Statics window owns toggle/flash and the popup. */
#ifndef LJ_UI_WIDGETS_W_SLOTBTN_H
#define LJ_UI_WIDGETS_W_SLOTBTN_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

typedef struct { lj_widget base; const char *caption; unsigned active:1; } lj_w_slotbtn;

lj_widget *lj_w_slotbtn_create(lj_widget *parent, const lj_ctl_desc *d);   /* caption from the desc */
void       lj_w_slotbtn_set(lj_widget *w, const char *caption, int active);

#endif /* LJ_UI_WIDGETS_W_SLOTBTN_H */
