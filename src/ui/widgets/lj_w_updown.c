/* lj_w_updown.c - TUpDown: classic spin pair, Position in Min..Max, Wrap, Associate (design 7.2, 7.3). */
#include "ui/widgets/lj_w_updown.h"

#include <stdio.h>
#include "ui/widgets/lj_widgets_lj.h"

static void paint(lj_widget *w, lj_surface *s)
{
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_w_spin_pair(s, w, ax, ay, w->w, w->h, (int)((lj_w_updown *)w)->pressed);
}

static void push_assoc(lj_w_updown *u)
{
    char buf[16];
    if (!u->assoc || !u->assoc_set) return;
    snprintf(buf, sizeof buf, "%d", u->pos);
    u->assoc_set(u->assoc, buf);
}

static int step(lj_w_updown *u, int dir)
{
    int p = u->pos + dir;
    if (p > u->max) p = u->wrap ? u->min : u->max;
    if (p < u->min) p = u->wrap ? u->max : u->min;
    u->dir = dir;
    if (p == u->pos) return 0;
    u->pos = p;
    push_assoc(u);
    lj_widget_invalidate(&u->base);
    lj_w_emit(&u->base, LJ_WEV_CLICK);                           /* TUpDown.OnClick(Button) after the position moved */
    return 1;
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_updown *u = (lj_w_updown *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
        u->pressed = m->y < w->h / 2 ? 1u : 2u;
        lj_w_emit(w, LJ_WEV_MOUSE_DOWN);
        step(u, u->pressed == 1 ? 1 : -1);                       /* the VCL acts on press (ponytail: no auto-repeat timer yet) */
        return 1;
    case LJ_EV_MOUSE_UP:
    case LJ_EV_MOUSE_LEAVE:
        if (u->pressed) { u->pressed = 0; lj_widget_invalidate(w); if (m->type == LJ_EV_MOUSE_UP) lj_w_emit(w, LJ_WEV_MOUSE_UP); }
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        if (m->wheel) step(u, m->wheel > 0 ? 1 : -1);
        return 1;
    default:
        return 0;
    }
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    if (!w->enabled || k->type != LJ_EV_KEY_DOWN) return 0;
    if (k->key == LJ_VK_UP)   { step((lj_w_updown *)w, 1);  return 1; }
    if (k->key == LJ_VK_DOWN) { step((lj_w_updown *)w, -1); return 1; }
    return 0;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL, NULL };

lj_widget *lj_w_updown_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_updown *u = lj_w_alloc(sizeof *u, parent, d, &vt, 1);
    if (d) { u->min = d->min; u->max = d->max; u->wrap = lj_desc_bool(d->wrap, 0); u->pos = d->position; }
    else u->max = 100;
    if (u->max < u->min) u->max = u->min;
    if (u->pos < u->min) u->pos = u->min;
    if (u->pos > u->max) u->pos = u->max;
    return &u->base;
}

int lj_w_updown_position(const lj_widget *w) { return ((const lj_w_updown *)w)->pos; }
int lj_w_updown_dir(const lj_widget *w)      { return ((const lj_w_updown *)w)->dir; }

int lj_w_updown_set_position(lj_widget *w, int pos)
{
    lj_w_updown *u = (lj_w_updown *)w;
    u->pos = pos < u->min ? u->min : pos > u->max ? u->max : pos;
    push_assoc(u);
    lj_widget_invalidate(w);
    return 0;
}

void lj_w_updown_bind(lj_widget *w, lj_widget *assoc, lj_w_text_setter set)
{
    lj_w_updown *u = (lj_w_updown *)w;
    u->assoc = assoc; u->assoc_set = set;
    push_assoc(u);
}
