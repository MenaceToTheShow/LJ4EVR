/* lj_w_palette.c - palette tile: bitmap or swatch, grey frame, thick black frame when selected (design 7.2). */
#include "ui/widgets/lj_w_palette.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static void frame(lj_surface *s, int x, int y, int w, int h, int t, uint32_t rgb)
{
    lj_fill(s, x, y, w, t, rgb); lj_fill(s, x, y + h - t, w, t, rgb);
    lj_fill(s, x, y, t, h, rgb); lj_fill(s, x + w - t, y, t, h, rgb);
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_palette *p = (lj_w_palette *)w;
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    if (p->tile) lj_blit_rgb(s, ax, ay, p->tile, 0);
    else lj_fill(s, ax, ay, w->w, w->h, p->swatch_rgb);
    if (p->selected) frame(s, ax, ay, w->w, w->h, 2, LJ_CLR_3DDKSHADOW);
    else frame(s, ax, ay, w->w, w->h, 1, LJ_CLR_BTNSHADOW);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_palette *p = (lj_w_palette *)w;
    if (!w->enabled || m->type != LJ_EV_MOUSE_DOWN || m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
    p->selected = 1;
    lj_widget_invalidate(w);
    lj_w_emit(w, LJ_WEV_CLICK);
    return 1;
}

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

lj_widget *lj_w_palette_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_palette *p = lj_w_alloc(sizeof *p, parent, d, &vt, 0);
    p->swatch_rgb = LJ_CLR_BTNHIGHLIGHT;
    if (d) { p->index = d->tag; p->swatch_rgb = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT); }
    else { p->base.w = LJ_PALETTE_W; p->base.h = LJ_PALETTE_H; }
    return &p->base;
}

void lj_w_palette_set_tile(lj_widget *w, const lj_surface *tile, uint32_t swatch_rgb)
{
    lj_w_palette *p = (lj_w_palette *)w;
    p->tile = tile; p->swatch_rgb = swatch_rgb & 0xFFFFFFu;
    lj_widget_invalidate(w);
}

void lj_w_palette_select(lj_widget *w, int on)
{
    ((lj_w_palette *)w)->selected = on ? 1u : 0u;
    lj_widget_invalidate(w);
}

int lj_w_palette_selected(const lj_widget *w) { return ((const lj_w_palette *)w)->selected; }
