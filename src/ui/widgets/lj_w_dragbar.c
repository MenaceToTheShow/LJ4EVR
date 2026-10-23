/* lj_w_dragbar.c - drag bar: arrows, sunken bar, proportional fill, centred caption, drag (design 7.2, 7.3, D6). */
#include "ui/widgets/lj_w_dragbar.h"

#include <stdio.h>
#include "core/lj_locale.h"
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static int bar_x(const lj_widget *w, int *iw)             /* interior x and width of the sunken bar */
{
    *iw = w->w - 2 * LJ_DRAGBAR_ARROW - 4;
    if (*iw < 1) *iw = 1;
    return LJ_DRAGBAR_ARROW + 3;
}

int lj_w_dragbar_text(const lj_widget *w, char *dst, size_t cap)
{
    const lj_w_dragbar *b = (const lj_w_dragbar *)w;
    int pct10 = b->max > 0 ? (b->value * 1000 + b->max / 2) / b->max : 0;
    if (b->caption) return snprintf(dst, cap, "%s %d%c%d%%", b->caption, pct10 / 10, lj_locale_decimal(), pct10 % 10) < (int)cap ? 0 : -1;
    return snprintf(dst, cap, "%d%c%d%%", pct10 / 10, lj_locale_decimal(), pct10 % 10) < (int)cap ? 0 : -1;
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_dragbar *b = (lj_w_dragbar *)w;
    char text[96];
    int ax, ay, iw, ix = bar_x(w, &iw), fw, cy = w->h / 2;
    uint32_t ink = w->enabled ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
    lj_edge_sunken(s, ax + LJ_DRAGBAR_ARROW + 1, ay, w->w - 2 * LJ_DRAGBAR_ARROW - 2, w->h, LJ_CLR_BTNSHADOW);
    fw = b->max > 0 ? (iw * b->value + b->max / 2) / b->max : 0;
    lj_fill(s, ax + ix, ay + 2, fw, w->h - 4, b->fill);
    lj_bevel1(s, ax, ay, w->w, w->h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);       /* the raised panel edge over the bar's top/bottom lines (scrollbar.gif) */
    lj_w_dragbar_text(w, text, sizeof text);
    lj_w_text_c(s, w, ax + ix, ay, iw, w->h, text, LJ_CLR_3DDKSHADOW, 0);
    /* arrow heads: apex at 10 / w-11, 9 rows, four full columns (measured) */
    lj_w_tri(s, ax + 10 + (b->pressed == 1), ay + cy + (b->pressed == 1), 5, 2, ink);
    lj_fill(s, ax + 15 + (b->pressed == 1), ay + cy - 4 + (b->pressed == 1), 4, 9, ink);
    lj_w_tri(s, ax + w->w - 11 + (b->pressed == 2), ay + cy + (b->pressed == 2), 5, 3, ink);
    lj_fill(s, ax + w->w - 19 + (b->pressed == 2), ay + cy - 4 + (b->pressed == 2), 4, 9, ink);
}

static int set(lj_w_dragbar *b, int v)
{
    if (v < 0) v = 0;
    if (v > b->max) v = b->max;
    if (v == b->value) return 0;
    b->value = v;
    lj_widget_invalidate(&b->base);
    lj_w_emit(&b->base, LJ_WEV_CHANGE);
    return 1;
}

static void set_from_x(lj_w_dragbar *b, int x)
{
    int iw, ix = bar_x(&b->base, &iw);
    int v = iw > 1 ? ((x - ix) * b->max + (iw - 1) / 2) / (iw - 1) : 0;
    set(b, v);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_dragbar *b = (lj_w_dragbar *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (!lj_w_inside(w, m->x, m->y) || (m->button != 1 && m->button != 2)) return 0;
        if (m->x < LJ_DRAGBAR_ARROW) {
            if (m->button == 1) { b->pressed = 1; set(b, b->value - 1); } else set(b, 0);
        } else if (m->x >= w->w - LJ_DRAGBAR_ARROW) {
            if (m->button == 1) { b->pressed = 2; set(b, b->value + 1); } else set(b, b->max);
        } else if (m->button == 1) {
            b->dragging = 1;
            set_from_x(b, m->x);
        }
        lj_widget_invalidate(w);
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (b->dragging) set_from_x(b, m->x);
        return b->dragging;
    case LJ_EV_MOUSE_UP:
    case LJ_EV_MOUSE_LEAVE:
        if (b->pressed || b->dragging) { b->pressed = 0; b->dragging = 0; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        if (m->wheel) set(b, b->value + (m->wheel > 0 ? 1 : -1));
        return 1;
    default:
        return 0;
    }
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_dragbar *b = (lj_w_dragbar *)w;
    if (!w->enabled || k->type != LJ_EV_KEY_DOWN) return 0;
    switch (k->key) {
    case LJ_VK_LEFT:  set(b, b->value - 1); return 1;
    case LJ_VK_RIGHT: set(b, b->value + 1); return 1;
    case LJ_VK_HOME:  set(b, 0); return 1;
    case LJ_VK_END:   set(b, b->max); return 1;
    default: return 0;
    }
}

static const lj_widget_vt vt = { paint, mouse, key, NULL, NULL };

lj_widget *lj_w_dragbar_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_dragbar *b = lj_w_alloc(sizeof *b, parent, d, &vt, 1);
    b->max = 255; b->fill = LJ_CLR_BTNHIGHLIGHT;
    if (d) {
        b->caption = d->caption;
        if (d->max > 0) b->max = d->max;
        b->value = d->position < 0 ? 0 : d->position > b->max ? b->max : d->position;
    } else {
        b->base.h = LJ_DRAGBAR_H;
    }
    return &b->base;
}

int lj_w_dragbar_value(const lj_widget *w) { return ((const lj_w_dragbar *)w)->value; }

int lj_w_dragbar_set_value(lj_widget *w, int v)
{
    lj_w_dragbar *b = (lj_w_dragbar *)w;
    b->value = v < 0 ? 0 : v > b->max ? b->max : v;
    lj_widget_invalidate(w);
    return 0;
}

void lj_w_dragbar_set_fill(lj_widget *w, uint32_t rgb)
{
    ((lj_w_dragbar *)w)->fill = rgb & 0xFFFFFFu;
    lj_widget_invalidate(w);
}
