/* lj_w_slotbtn.c - statics slot button, paint only (design 7.3). */
#include "ui/widgets/lj_w_slotbtn.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_slotbtn *b = (lj_w_slotbtn *)w;
    int ax, ay, p = b->active;
    lj_widget_abs(w, &ax, &ay);
    lj_w_button(s, ax, ay, w->w, w->h, w->color, p);
    lj_w_text_fit(s, w, ax + 4 + p, ay + (w->h - lj_w_font_of(w)->height) / 2 + p, b->caption, LJ_CLR_3DDKSHADOW, w->w - 8);
}

static const lj_widget_vt vt = { paint, NULL, NULL, NULL };

lj_widget *lj_w_slotbtn_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_slotbtn *b = lj_w_alloc(sizeof *b, parent, d, &vt, 0);
    if (d) b->caption = d->caption;
    return &b->base;
}

void lj_w_slotbtn_set(lj_widget *w, const char *caption, int active)
{
    lj_w_slotbtn *b = (lj_w_slotbtn *)w;
    b->caption = caption; b->active = active ? 1u : 0u;
    lj_widget_invalidate(w);
}
