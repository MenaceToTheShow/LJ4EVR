/* lj_w_spinedit.c - TSpinEdit: sunken edit + spin buttons; digits, backspace, arrows, wheel (design 7.2, 7.3).
 * ponytail: no caret or selection; the number is edited as a value (a digit appends, backspace drops the last digit). */
#include "ui/widgets/lj_w_spinedit.h"

#include <stdio.h>
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_spinedit *e = (lj_w_spinedit *)w;
    const lj_font *f = lj_w_font_of(w);
    char buf[16];
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, w->enabled ? (w->desc ? lj_desc_rgb(w->desc->color, LJ_CLR_BTNHIGHLIGHT) : LJ_CLR_BTNHIGHLIGHT) : LJ_CLR_BTNFACE);
    snprintf(buf, sizeof buf, "%d", e->value);
    if (w->enabled) lj_text(s, f, ax + 3, ay + (w->h - f->height) / 2, buf, LJ_CLR_3DDKSHADOW, 0);
    else lj_text_disabled(s, f, ax + 3, ay + (w->h - f->height) / 2, buf, 0);
    lj_w_spin_pair(s, w, ax + w->w - 2 - LJ_W_SPIN_BTN_W, ay + 2, LJ_W_SPIN_BTN_W, w->h - 4, (int)e->pressed);
}

static int set(lj_w_spinedit *e, int v)
{
    if (v < e->min_value) v = e->min_value;
    if (v > e->max_value) v = e->max_value;
    if (v == e->value) return 0;
    e->value = v;
    lj_widget_invalidate(&e->base);
    lj_w_emit(&e->base, LJ_WEV_CHANGE);
    return 1;
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_spinedit *e = (lj_w_spinedit *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
        lj_widget_set_focus(w);
        if (m->x >= w->w - 2 - LJ_W_SPIN_BTN_W) {
            e->pressed = m->y < 2 + (w->h - 4) / 2 ? 1u : 2u;
            lj_widget_invalidate(w);
            set(e, e->value + (e->pressed == 1 ? 1 : -1));
        }
        return 1;
    case LJ_EV_MOUSE_UP:
    case LJ_EV_MOUSE_LEAVE:
        if (e->pressed) { e->pressed = 0; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        if (m->wheel) set(e, e->value + (m->wheel > 0 ? 1 : -1));
        return 1;
    default:
        return 0;
    }
}

static int digits(int v) { int n = 1; while (v >= 10) { v /= 10; n++; } return n; }

static int key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_spinedit *e = (lj_w_spinedit *)w;
    if (!w->enabled) return 0;
    if (k->type == LJ_EV_KEY_DOWN) {
        switch (k->key) {
        case LJ_VK_UP:   set(e, e->value + 1); return 1;
        case LJ_VK_DOWN: set(e, e->value - 1); return 1;
        case LJ_VK_HOME: set(e, e->min_value); return 1;
        case LJ_VK_END:  set(e, e->max_value); return 1;
        case LJ_VK_BACK: set(e, e->value / 10); return 1;
        default: return 0;
        }
    }
    if (k->type == LJ_EV_CHAR && k->ch >= '0' && k->ch <= '9') {
        if (e->max_length > 0 && e->value != 0 && digits(e->value) >= e->max_length) return 1;   /* MaxLength: the keystroke is swallowed */
        set(e, e->value * 10 + (k->ch - '0'));
        return 1;
    }
    return 0;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL };

lj_widget *lj_w_spinedit_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_spinedit *e = lj_w_alloc(sizeof *e, parent, d, &vt, 1);
    if (d) { e->min_value = d->min_value; e->max_value = d->max_value; e->max_length = d->max_length; e->value = d->value; }
    else e->max_value = 100;
    if (e->max_value < e->min_value) e->max_value = e->min_value;
    if (e->value < e->min_value) e->value = e->min_value;
    if (e->value > e->max_value) e->value = e->max_value;
    return &e->base;
}

int lj_w_spinedit_value(const lj_widget *w) { return ((const lj_w_spinedit *)w)->value; }

int lj_w_spinedit_set_value(lj_widget *w, int v)
{
    lj_w_spinedit *e = (lj_w_spinedit *)w;
    e->value = v < e->min_value ? e->min_value : v > e->max_value ? e->max_value : v;
    lj_widget_invalidate(w);
    return 0;
}
