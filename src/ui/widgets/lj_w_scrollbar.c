/* lj_w_scrollbar.c - TScrollBar: the embedded classic scrollbar of
 * lj_widgets_lists.c as a widget (Kind = sbHorizontal/sbVertical, Min/Max/
 * Position/PageSize); OnScroll then OnChange when the position moves. */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>

static void fit(lj_w_scrollbar *b) { b->sb.x = 0; b->sb.y = 0; b->sb.w = b->base.w; b->sb.h = b->base.h; }

static void sbw_paint(lj_widget *w, lj_surface *s)
{
    lj_w_scrollbar *b = (lj_w_scrollbar *)w;
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    fit(b);
    lj_w_sbar_paint(s, &b->sb, ax, ay, w->enabled);
}

static void moved(lj_widget *w)
{
    lj_widget_invalidate(w);
    lj_w_emit(w, LJ_WEV_SCROLL);
    lj_w_emit(w, LJ_WEV_CHANGE);
}

static int sbw_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_scrollbar *b = (lj_w_scrollbar *)w;
    int r;
    if (!w->enabled) return 0;
    fit(b);
    if (m->type == LJ_EV_MOUSE_WHEEL) { if (lj_w_sbar_set(&b->sb, b->sb.pos - m->wheel)) moved(w); return 1; }
    if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1) lj_widget_set_focus(w);
    r = lj_w_sbar_mouse(&b->sb, m);
    if (r == 1) moved(w);
    return r >= 0;
}

static int sbw_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_scrollbar *b = (lj_w_scrollbar *)w;
    int along = b->sb.vertical ? (k->key == LJ_VK_UP || k->key == LJ_VK_DOWN) : (k->key == LJ_VK_LEFT || k->key == LJ_VK_RIGHT);
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled) return 0;
    if (lj_w_sbar_key(&b->sb, k->key)) moved(w);
    return along || k->key == LJ_VK_PRIOR || k->key == LJ_VK_NEXT || k->key == LJ_VK_HOME || k->key == LJ_VK_END;
}

static const lj_widget_vt sbw_vt = { sbw_paint, sbw_mouse, sbw_key, NULL };

lj_widget *lj_w_scrollbar_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_scrollbar *b = lj_w_alloc(sizeof *b, parent, d, &sbw_vt, 1);
    b->sb.max = 100; b->sb.page = 1;
    if (d) {
        b->sb.min = d->min; b->sb.max = d->max > d->min ? d->max : 100; b->sb.pos = d->position;
        b->sb.page = d->page_size > 0 ? d->page_size : 1;
        b->sb.vertical = d->kind_str && strcmp(d->kind_str, "sbVertical") == 0;
    }
    lj_w_sbar_set(&b->sb, b->sb.pos);
    return &b->base;
}

void lj_w_scrollbar_set(lj_widget *w, int pos)
{
    if (lj_w_sbar_set(&((lj_w_scrollbar *)w)->sb, pos)) lj_widget_invalidate(w);
}
