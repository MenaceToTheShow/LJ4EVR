/* lj_w_bevel.c - TBevel as TBevel.Paint draws it: Style bsLowered (default) = #808080 / white,
 * bsRaised the reverse; Shape bsBox (default), bsFrame (the two rects offset +1,+1),
 * bsTopLine/bsBottomLine/bsLeftLine/bsRightLine (one line pair), bsSpacer nothing (7.3). */
#include "ui/widgets/lj_w_bevel.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "gfx/lj_draw.h"

static void paint(lj_widget *w, lj_surface *s)
{
    const lj_ctl_desc *d = w->desc;
    const char *shape = d && d->shape ? d->shape : "bsBox";
    uint32_t c1 = LJ_CLR_BTNSHADOW, c2 = LJ_CLR_BTNHIGHLIGHT;
    int x, y;
    lj_widget_abs(w, &x, &y);
    if (d && d->style && strcmp(d->style, "bsRaised") == 0) { c1 = LJ_CLR_BTNHIGHLIGHT; c2 = LJ_CLR_BTNSHADOW; }
    if (strcmp(shape, "bsFrame") == 0) {
        lj_bevel1(s, x, y, w->w - 1, w->h - 1, c1, c1);
        lj_bevel1(s, x + 1, y + 1, w->w - 1, w->h - 1, c2, c2);
    } else if (strcmp(shape, "bsTopLine") == 0) {
        lj_hline(s, x, y, w->w, c1); lj_hline(s, x, y + 1, w->w, c2);
    } else if (strcmp(shape, "bsBottomLine") == 0) {
        lj_hline(s, x, y + w->h - 2, w->w, c1); lj_hline(s, x, y + w->h - 1, w->w, c2);
    } else if (strcmp(shape, "bsLeftLine") == 0) {
        lj_vline(s, x, y, w->h, c1); lj_vline(s, x + 1, y, w->h, c2);
    } else if (strcmp(shape, "bsRightLine") == 0) {
        lj_vline(s, x + w->w - 2, y, w->h, c1); lj_vline(s, x + w->w - 1, y, w->h, c2);
    } else if (strcmp(shape, "bsBox") == 0) {
        lj_bevel1(s, x, y, w->w, w->h, c1, c2);
    }
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

lj_widget *lj_w_bevel_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_alloc(sizeof(lj_widget), parent, d, &vt, 0);
}
