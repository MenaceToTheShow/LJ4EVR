/* lj_w_radiogroup.c - TRadioGroup: a TGroupBox whose Items are code-built TRadioButton
 * children laid out as TCustomRadioGroup.ArrangeButtons does (Columns, column-major,
 * 8 px left inset, text height top margin). ItemIndex follows the checked child. */
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

static int radio_at(lj_widget *w, lj_widget *child)
{
    lj_widget *c;
    int i = 0;
    for (c = w->first; c; c = c->next, i++)
        if (c == child) return i;
    return -1;
}

static void layout(lj_widget *w)
{
    const lj_font *f = lj_w_font_of(w);
    lj_widget *c;
    int n = 0, cols = w->desc && w->desc->columns_count > 0 ? w->desc->columns_count : 1, per_col, bw, bh, top, i = 0;
    for (c = w->first; c; c = c->next) n++;
    if (!n) return;
    per_col = (n + cols - 1) / cols;
    bw = (w->w - 10) / cols;
    bh = (w->h - f->height - 5) / per_col;
    top = f->height + 1 + ((w->h - f->height - 5) % per_col) / 2;
    for (c = w->first; c; c = c->next, i++) {
        c->x = (i / per_col) * bw + 8;
        c->y = (i % per_col) * bh + top;
        c->w = bw; c->h = bh;
    }
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { lj_w_groupbox_paint, mouse, NULL, layout };

void lj_w_radiogroup_child_checked(lj_widget *group, lj_widget *child)
{
    int i = radio_at(group, child);
    if (i < 0 || ((lj_w_cap *)group)->index == i) return;
    ((lj_w_cap *)group)->index = i;
    lj_w_emit(group, LJ_WEV_CLICK);
}

int lj_w_radiogroup_index(const lj_widget *w)
{
    return w && w->kind == LJ_K_RADIOGROUP ? ((const lj_w_cap *)w)->index : -1;
}

int lj_w_radiogroup_set_index(lj_widget *w, int i)
{
    lj_widget *c;
    int k = 0;
    if (!w || w->kind != LJ_K_RADIOGROUP) return -1;
    for (c = w->first; c; c = c->next, k++)
        if (k == i) { lj_w_radiobutton_click(c); return 0; }
    return -1;
}

lj_widget *lj_w_radiogroup_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_cap *g = lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
    int i;
    g->index = -1;
    if (d)
        for (i = 0; i < d->nitems; i++) {
            lj_widget *rb = lj_w_radiobutton_create(&g->w, NULL);
            lj_w_set_caption(rb, d->items[i]);
            if (i == d->item_index) { ((lj_w_cap *)rb)->checked = 1; rb->tabstop = 1; g->index = i; }
        }
    layout(&g->w);
    return &g->w;
}
