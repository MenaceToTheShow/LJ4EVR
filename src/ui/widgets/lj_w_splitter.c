/* lj_w_splitter.c - TSplitter: a bar aligned to one edge that resizes the aligned
 * sibling it touches (the control to its left for alLeft, above for alTop ...), live
 * (ResizeStyle rsUpdate, every LJ2 use), both sides kept >= MinSize (default 30),
 * OnMoved on release. Beveled draws a 1-px raised edge. */
#include "ui/widgets/lj_w_splitter.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

typedef struct { lj_widget w; lj_widget *target; int start, size0, min_size; unsigned pressed:1, beveled:1; } lj_w_split;

static int horizontal(const lj_widget *w) { return w->align == LJ_AL_LEFT || w->align == LJ_AL_RIGHT; }

static lj_widget *find_target(const lj_widget *w)
{
    lj_widget *c;
    if (!w->parent) return NULL;
    for (c = w->parent->first; c; c = c->next) {
        if (c == w || !c->visible || c->align != w->align) continue;
        if ((w->align == LJ_AL_LEFT && c->x + c->w == w->x) || (w->align == LJ_AL_RIGHT && c->x == w->x + w->w) ||
            (w->align == LJ_AL_TOP && c->y + c->h == w->y) || (w->align == LJ_AL_BOTTOM && c->y == w->y + w->h)) return c;
    }
    return NULL;
}

static void paint(lj_widget *w, lj_surface *s)
{
    int x, y;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, w->color);
    if (((lj_w_split *)w)->beveled) lj_bevel1(s, x, y, w->w, w->h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_split *sp = (lj_w_split *)w;
    int pos = horizontal(w) ? w->x + m->x : w->y + m->y;      /* parent coordinates: the bar moves under the drag */
    if (!w->enabled || !w->parent) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1) return 0;
        sp->target = find_target(w);
        if (!sp->target) return 0;
        sp->start = pos;
        sp->size0 = horizontal(w) ? sp->target->w : sp->target->h;
        sp->pressed = 1;
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (sp->pressed && sp->target) {
            int delta = pos - sp->start, size, room;
            if (w->align == LJ_AL_RIGHT || w->align == LJ_AL_BOTTOM) delta = -delta;
            room = (horizontal(w) ? w->parent->w - w->w : w->parent->h - w->h) - sp->min_size;
            size = sp->size0 + delta;
            if (size < sp->min_size) size = sp->min_size;
            if (size > room) size = room;
            if (horizontal(w)) sp->target->w = size; else sp->target->h = size;
            lj_widget_layout(w->parent);
            lj_widget_invalidate(w->parent);
        }
        return sp->pressed;
    case LJ_EV_MOUSE_UP:
        if (!sp->pressed) return 0;
        sp->pressed = 0;
        lj_w_emit(w, LJ_WEV_MOVED);
        return 1;
    default:
        return 0;
    }
}

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

lj_widget *lj_w_splitter_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_split *sp = lj_w_alloc(sizeof(lj_w_split), parent, d, &vt, 0);
    sp->min_size = d && d->min_size > 0 ? d->min_size : 30;
    if (d) {
        sp->beveled = lj_desc_bool(d->beveled, 0) != 0;
        if (!(d->color & LJ_COLOR_SET) && parent && lj_desc_bool(d->parent_color, 0)) sp->w.color = parent->color;
    }
    return &sp->w;
}
