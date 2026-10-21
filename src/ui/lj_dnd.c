/* lj_dnd.c - drag and drop dispatch through the host's window_at query (design 7.9). */
#include "ui/lj_dnd.h"

#include <string.h>

static void fire(lj_widget *w, int ev) { if (w && w->on_event) w->on_event(w, ev, w->ud); }

int lj_dnd_begin(lj_widget *source, int kind, int number)
{
    lj_window *win = lj_window_of(source);
    lj_dnd *d;
    if (!win) return -1;
    d = lj_window_dnd(win);
    if (d->active) return -1;
    memset(d, 0, sizeof *d);
    d->active = 1; d->kind = kind; d->number = number; d->source = source;
    fire(source, LJ_WEV_START_DRAG);
    return 0;
}

/* The widget under a screen point and the window that owns it; headless the
 * source window is the only one and sx,sy are taken as its surface coords. */
static lj_widget *target_at(lj_window *src, int sx, int sy, lj_window **twin, int *rx, int *ry)
{
    lj_window *w;
    lj_widget *root, *hit;
    int x = sx, y = sy, ax, ay;
    w = lj_window_at_screen(src, sx, sy, &x, &y);
    *twin = w;
    if (!w) return NULL;
    root = lj_window_root(w);
    hit = lj_widget_hit(root, x - root->x, y - root->y);
    if (!hit) return NULL;
    lj_widget_abs(hit, &ax, &ay);
    *rx = x - ax; *ry = y - ay;
    return hit;
}

static void dispatch(lj_window *src, lj_widget *target, lj_window *twin, int rx, int ry, int ev)
{
    lj_dnd *s = lj_window_dnd(src), *t;
    if (!target || !twin) return;
    t = lj_window_dnd(twin);
    if (t != s) { t->kind = s->kind; t->number = s->number; t->source = s->source; }
    t->target = target; t->x = rx; t->y = ry;
    fire(target, ev);
    if (t != s) t->target = NULL;
}

void lj_dnd_move(lj_window *src, int sx, int sy)
{
    lj_window *twin;
    int rx = 0, ry = 0;
    lj_widget *t;
    if (!src || !lj_window_dnd(src)->active) return;
    t = target_at(src, sx, sy, &twin, &rx, &ry);
    dispatch(src, t, twin, rx, ry, LJ_WEV_DRAG_OVER);
}

void lj_dnd_end(lj_window *src, int sx, int sy, int drop)
{
    lj_dnd *d;
    lj_window *twin;
    int rx = 0, ry = 0;
    lj_widget *t, *source;
    if (!src) return;
    d = lj_window_dnd(src);
    if (!d->active) return;
    source = d->source;
    if (drop) {
        t = target_at(src, sx, sy, &twin, &rx, &ry);
        dispatch(src, t, twin, rx, ry, LJ_WEV_DRAG_DROP);
    }
    fire(source, LJ_WEV_END_DRAG);
    memset(d, 0, sizeof *d);
}

int lj_dnd_info(lj_window *w, int *kind, int *number, int *x, int *y)
{
    lj_dnd *d;
    if (!w) return -1;
    d = lj_window_dnd(w);
    if (!d->active && !d->target) return -1;
    if (kind) *kind = d->kind;
    if (number) *number = d->number;
    if (x) *x = d->x;
    if (y) *y = d->y;
    return 0;
}

int lj_dnd_active(lj_window *w) { return w ? lj_window_dnd(w)->active : 0; }
