/* lj_w_udbutton.h - TUdButton, Martin's 20x21 up/down spinner beside a plain TEdit
 * (UdScene/UdFade on TSEQUENCEFORM, 17 uses). No Min/Max/Position in any DFM: it is a
 * pulse emitter whose OnChange handler reads the direction and edits the field itself. */
#ifndef LJ_UI_WIDGETS_W_UDBUTTON_H
#define LJ_UI_WIDGETS_W_UDBUTTON_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

typedef struct { lj_widget base; int delta; unsigned pressed:2; } lj_w_udbutton;   /* delta: +1 up, -1 down, of the last click */

lj_widget *lj_w_udbutton_create(lj_widget *parent, const lj_ctl_desc *d);
int        lj_w_udbutton_delta(const lj_widget *w);                        /* read inside the OnChange handler */

#endif /* LJ_UI_WIDGETS_W_UDBUTTON_H */
