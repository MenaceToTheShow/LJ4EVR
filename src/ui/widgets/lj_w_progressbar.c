/* lj_w_progressbar.c - TProgressBar: 1-px sunken frame (#808080 top/left, white
 * bottom/right), clBtnFace interior, navy #000080 blocks two thirds of the
 * inner height wide with 2-px gaps (the classic look); Smooth = one solid bar;
 * Orientation = pbVertical fills bottom-up. */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>

static void pb_paint(lj_widget *w, lj_surface *s)
{
    lj_w_progressbar *p = (lj_w_progressbar *)w;
    int ax, ay, range = p->max - p->min, iw = w->w - 4, ih = w->h - 4, len, bw, gap = 2, i, x0, y0;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, LJ_CLR_BTNFACE);
    lj_bevel1(s, ax, ay, w->w, w->h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
    if (range <= 0 || iw <= 0 || ih <= 0) return;
    x0 = ax + 2; y0 = ay + 2;
    len = (p->pos - p->min) * (p->vertical ? ih : iw) / range;
    if (p->smooth) {
        if (p->vertical) lj_fill(s, x0, y0 + ih - len, iw, len, LJ_CLR_HIGHLIGHT);
        else lj_fill(s, x0, y0, len, ih, LJ_CLR_HIGHLIGHT);
        return;
    }
    bw = (p->vertical ? iw : ih) * 2 / 3;
    if (bw < 1) bw = 1;
    for (i = 0; (i + 1) * (bw + gap) - gap <= len; i++) {
        if (p->vertical) lj_fill(s, x0, y0 + ih - (i + 1) * (bw + gap) + gap, iw, bw, LJ_CLR_HIGHLIGHT);
        else lj_fill(s, x0 + i * (bw + gap), y0, bw, ih, LJ_CLR_HIGHLIGHT);
    }
}

static const lj_widget_vt pb_vt = { pb_paint, NULL, NULL, NULL };

lj_widget *lj_w_progressbar_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_progressbar *p = lj_w_alloc(sizeof *p, parent, d, &pb_vt, 0);
    p->max = 100; p->step = 10;
    if (d) {
        p->min = d->min; p->max = d->max > d->min ? d->max : 100; p->pos = d->position; p->step = d->step > 0 ? d->step : 10;
        p->smooth = lj_desc_bool(d->smooth, 0) != 0;
        p->vertical = d->orientation && strcmp(d->orientation, "pbVertical") == 0;
    }
    lj_w_progressbar_set(&p->base, p->pos);
    return &p->base;
}

void lj_w_progressbar_set(lj_widget *w, int pos)
{
    lj_w_progressbar *p = (lj_w_progressbar *)w;
    if (pos < p->min) pos = p->min;
    if (pos > p->max) pos = p->max;
    if (pos != p->pos) lj_widget_invalidate(w);
    p->pos = pos;
}
