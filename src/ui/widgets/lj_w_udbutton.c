/* lj_w_udbutton.c - TUdButton: two stacked raised arrow buttons, OnChange with a direction (design 7.2, spike A updown()). */
#include "ui/widgets/lj_w_udbutton.h"

#include "ui/widgets/lj_widgets_lj.h"

static void paint(lj_widget *w, lj_surface *s)
{
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_w_spin_pair(s, w, ax, ay, w->w, w->h, (int)((lj_w_udbutton *)w)->pressed);
}

static void pulse(lj_w_udbutton *u, int delta)
{
    u->delta = delta;
    lj_w_emit(&u->base, LJ_WEV_CHANGE);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_udbutton *u = (lj_w_udbutton *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
        u->pressed = m->y < w->h / 2 ? 1u : 2u;
        lj_widget_invalidate(w);
        pulse(u, u->pressed == 1 ? 1 : -1);
        return 1;
    case LJ_EV_MOUSE_UP:
    case LJ_EV_MOUSE_LEAVE:
        if (u->pressed) { u->pressed = 0; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        if (m->wheel) pulse(u, m->wheel > 0 ? 1 : -1);
        return 1;
    default:
        return 0;
    }
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    if (!w->enabled || k->type != LJ_EV_KEY_DOWN) return 0;
    if (k->key == LJ_VK_UP)   { pulse((lj_w_udbutton *)w, 1);  return 1; }
    if (k->key == LJ_VK_DOWN) { pulse((lj_w_udbutton *)w, -1); return 1; }
    return 0;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL };

lj_widget *lj_w_udbutton_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_udbutton *u = lj_w_alloc(sizeof *u, parent, d, &vt, 0);
    return &u->base;
}

int lj_w_udbutton_delta(const lj_widget *w) { return ((const lj_w_udbutton *)w)->delta; }
