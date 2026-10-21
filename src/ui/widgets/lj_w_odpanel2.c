/* lj_w_odpanel2.c - TODPanel2, Martin's list host: every DFM use holds one alClient
 * child at (1,1) w-2 x h-2, so it is a face with a 1-px lowered frame. */
#include "ui/widgets/lj_w_odpanel2.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

static void paint(lj_widget *w, lj_surface *s)
{
    int x, y;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, w->color);
    lj_bevel1(s, x, y, w->w, w->h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

lj_widget *lj_w_odpanel2_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_alloc(sizeof(lj_widget), parent, d, &vt, 0);
}
