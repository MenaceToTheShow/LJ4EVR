/* lj_w_updown.h - TUpDown (15x21 in the DFMs): two stacked raised arrow buttons
 * driving a Position in Min..Max with Wrap; the Associate edit shows the number. */
#ifndef LJ_UI_WIDGETS_W_UPDOWN_H
#define LJ_UI_WIDGETS_W_UPDOWN_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

typedef int (*lj_w_text_setter)(lj_widget *w, const char *text);   /* the Associate's text setter (the basic group's edit) */

typedef struct {
    lj_widget base;
    int pos, min, max, wrap, dir;          /* dir: +1/-1 of the last step, for the OnClick(Button) handler */
    unsigned pressed:2;                    /* 0 none, 1 top half, 2 bottom half */
    lj_widget *assoc; lj_w_text_setter assoc_set;
} lj_w_updown;

lj_widget *lj_w_updown_create(lj_widget *parent, const lj_ctl_desc *d);   /* min/max/position/wrap from the desc */
int  lj_w_updown_position(const lj_widget *w);
int  lj_w_updown_set_position(lj_widget *w, int pos);                      /* clamps; updates the Associate; no event */
void lj_w_updown_bind(lj_widget *w, lj_widget *assoc, lj_w_text_setter set);   /* Associate = <edit>: writes the position now and after every step */
int  lj_w_updown_dir(const lj_widget *w);                                  /* +1 after an up step, -1 after a down step */

#endif /* LJ_UI_WIDGETS_W_UPDOWN_H */
