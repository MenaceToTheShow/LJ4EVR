/* lj_w_cuerow.c - cue grid row, paint only (design 7.3, 7.7). */
#include "ui/widgets/lj_w_cuerow.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cuerow *r = (lj_w_cuerow *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, i, x, ty;
    uint32_t rgb = r->selected ? LJ_CLR_BTNHIGHLIGHT : r->text_rgb;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, r->selected ? LJ_CLR_HIGHLIGHT : (w->desc ? lj_desc_rgb(w->desc->color, LJ_CLR_BTNHIGHLIGHT) : LJ_CLR_BTNHIGHLIGHT));
    ty = ay + (w->h - f->height) / 2;
    for (i = 0, x = 0; i < r->ncells && r->cells && r->col_w && x < w->w; x += r->col_w[i], i++)
        lj_w_text_fit(s, w, ax + x + 2, ty, r->cells[i], rgb, (r->col_w[i] < w->w - x ? r->col_w[i] : w->w - x) - 4);
}

static const lj_widget_vt vt = { paint, NULL, NULL, NULL, NULL };

lj_widget *lj_w_cuerow_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_cuerow *r = lj_w_alloc(sizeof *r, parent, d, &vt, 0);
    r->text_rgb = LJ_CLR_3DDKSHADOW;
    if (!d) r->base.h = LJ_CUEROW_H;
    return &r->base;
}

void lj_w_cuerow_set(lj_widget *w, const char *const *cells, const int *col_w, int n, uint32_t text_rgb, int selected)
{
    lj_w_cuerow *r = (lj_w_cuerow *)w;
    r->cells = cells; r->col_w = col_w; r->ncells = n; r->text_rgb = text_rgb & 0xFFFFFFu; r->selected = selected ? 1u : 0u;
    lj_widget_invalidate(w);
}
