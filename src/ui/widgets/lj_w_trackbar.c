/* lj_w_trackbar.c - TTrackBar: 4-px sunken channel, 11x21 raised thumb pointing
 * at the 3-px black ticks (every Frequency, tmBottomRight), arrows/page/home/end,
 * drag and page-click with the mouse, focus rect round the client; OnChange.
 * Orientation = trVertical mirrors the axes (thumb points right). */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>

#define TB_MARGIN 2
#define TB_HALF   5            /* thumb half-width along the axis */
#define TB_LEN    21           /* thumb length across the axis, point included */

static int axis_len(const lj_w_trackbar *t) { return t->vertical ? t->base.h : t->base.w; }
static int span(const lj_w_trackbar *t) { int s = axis_len(t) - 2 * (TB_MARGIN + TB_HALF) - 1; return s < 1 ? 1 : s; }

int lj_w_trackbar_thumb_pos(const lj_widget *w)
{
    const lj_w_trackbar *t = (const lj_w_trackbar *)w;
    int range = t->max - t->min;
    return TB_MARGIN + TB_HALF + (range > 0 ? (t->pos - t->min) * span(t) / range : 0);
}

static int pos_at(const lj_w_trackbar *t, int c)
{
    int range = t->max - t->min, s = span(t);
    int p = t->min + ((c - TB_MARGIN - TB_HALF) * range + s / 2) / s;
    return p < t->min ? t->min : (p > t->max ? t->max : p);
}

static void thumb(lj_surface *s, int cx, int cy, int vertical)
{
    int r;
    if (!vertical) {
        int x = cx - TB_HALF, y = cy, body = TB_LEN - 5;
        lj_button_raised(s, x, y, 2 * TB_HALF + 1, body);
        lj_hline(s, x + 1, y + body - 2, 2 * TB_HALF - 1, LJ_CLR_BTNFACE);
        lj_hline(s, x, y + body - 1, 2 * TB_HALF + 1, LJ_CLR_BTNFACE);
        lj_vline(s, x, y + body - 2, 2, LJ_CLR_BTNHIGHLIGHT);
        lj_vline(s, x + 2 * TB_HALF, y + body - 2, 2, LJ_CLR_3DDKSHADOW);
        lj_vline(s, x + 2 * TB_HALF - 1, y + body - 2, 2, LJ_CLR_BTNSHADOW);
        for (r = 0; r < 5; r++) {
            int half = 4 - r, ry = y + body + r;
            lj_hline(s, cx - half, ry, 2 * half + 1, LJ_CLR_BTNFACE);
            lj_fill(s, cx - half - 1, ry, 1, 1, LJ_CLR_BTNHIGHLIGHT);
            lj_fill(s, cx + half + 1, ry, 1, 1, LJ_CLR_3DDKSHADOW);
            if (half > 0) lj_fill(s, cx + half, ry, 1, 1, LJ_CLR_BTNSHADOW);
        }
    } else {
        int x = cx, y = cy - TB_HALF, body = TB_LEN - 5;
        lj_button_raised(s, x, y, body, 2 * TB_HALF + 1);
        lj_vline(s, x + body - 2, y + 1, 2 * TB_HALF - 1, LJ_CLR_BTNFACE);
        lj_vline(s, x + body - 1, y, 2 * TB_HALF + 1, LJ_CLR_BTNFACE);
        lj_hline(s, x + body - 2, y, 2, LJ_CLR_BTNHIGHLIGHT);
        lj_hline(s, x + body - 2, y + 2 * TB_HALF, 2, LJ_CLR_3DDKSHADOW);
        lj_hline(s, x + body - 2, y + 2 * TB_HALF - 1, 2, LJ_CLR_BTNSHADOW);
        for (r = 0; r < 5; r++) {
            int half = 4 - r, rx = x + body + r;
            lj_vline(s, rx, cy - half, 2 * half + 1, LJ_CLR_BTNFACE);
            lj_fill(s, rx, cy - half - 1, 1, 1, LJ_CLR_BTNHIGHLIGHT);
            lj_fill(s, rx, cy + half + 1, 1, 1, LJ_CLR_3DDKSHADOW);
            if (half > 0) lj_fill(s, rx, cy + half, 1, 1, LJ_CLR_BTNSHADOW);
        }
    }
}

static void tb_paint(lj_widget *w, lj_surface *s)
{
    lj_w_trackbar *t = (lj_w_trackbar *)w;
    int ax, ay, c = lj_w_trackbar_thumb_pos(w), v, range = t->max - t->min, freq = t->freq > 0 ? t->freq : 1;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
    if (!t->vertical) {
        lj_edge_sunken(s, ax + TB_MARGIN, ay + TB_MARGIN + 8, w->w - 2 * TB_MARGIN, 4, LJ_CLR_BTNFACE);
        if (!t->ticks_none)
            for (v = t->min; v <= t->max; v += freq) {
                int tx = ax + TB_MARGIN + TB_HALF + (range > 0 ? (v - t->min) * span(t) / range : 0);
                lj_vline(s, tx, ay + TB_MARGIN + TB_LEN + 2, 3, LJ_CLR_3DDKSHADOW);
                if (v != t->max && v + freq > t->max) v = t->max - freq;   /* always a tick at Max */
            }
        thumb(s, ax + c, ay + TB_MARGIN, 0);
    } else {
        lj_edge_sunken(s, ax + TB_MARGIN + 8, ay + TB_MARGIN, 4, w->h - 2 * TB_MARGIN, LJ_CLR_BTNFACE);
        if (!t->ticks_none)
            for (v = t->min; v <= t->max; v += freq) {
                int ty = ay + TB_MARGIN + TB_HALF + (range > 0 ? (v - t->min) * span(t) / range : 0);
                lj_hline(s, ax + TB_MARGIN + TB_LEN + 2, ty, 3, LJ_CLR_3DDKSHADOW);
                if (v != t->max && v + freq > t->max) v = t->max - freq;
            }
        thumb(s, ax + TB_MARGIN, ay + c, 1);
    }
    if (w->focused) lj_focus_rect(s, ax, ay, w->w, w->h);
}

void lj_w_trackbar_set(lj_widget *w, int pos)
{
    lj_w_trackbar *t = (lj_w_trackbar *)w;
    if (pos < t->min) pos = t->min;
    if (pos > t->max) pos = t->max;
    if (pos != t->pos) lj_widget_invalidate(w);
    t->pos = pos;
}

static void change(lj_widget *w, int pos)
{
    int old = ((lj_w_trackbar *)w)->pos;
    lj_w_trackbar_set(w, pos);
    if (((lj_w_trackbar *)w)->pos != old) lj_w_emit(w, LJ_WEV_CHANGE);
}

static int tb_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_trackbar *t = (lj_w_trackbar *)w;
    int c = t->vertical ? m->y : m->x, cross = t->vertical ? m->x : m->y, tc = lj_w_trackbar_thumb_pos(w);
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1) return 0;
        lj_widget_set_focus(w);
        if (cross >= TB_MARGIN && cross < TB_MARGIN + TB_LEN && c >= tc - TB_HALF && c <= tc + TB_HALF) { t->drag = 1; return 1; }
        if (cross >= TB_MARGIN && cross < TB_MARGIN + TB_LEN) change(w, c < tc ? t->pos - t->page : t->pos + t->page);
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (!t->drag) return 0;
        change(w, pos_at(t, c));
        return 1;
    case LJ_EV_MOUSE_UP:
        if (!t->drag) return 0;
        t->drag = 0;
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        change(w, t->pos - m->wheel);
        return 1;
    default: return 0;
    }
}

static int tb_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_trackbar *t = (lj_w_trackbar *)w;
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled) return 0;
    switch (k->key) {
    case LJ_VK_LEFT: case LJ_VK_UP:    change(w, t->pos - 1); return 1;
    case LJ_VK_RIGHT: case LJ_VK_DOWN: change(w, t->pos + 1); return 1;
    case LJ_VK_PRIOR: change(w, t->pos - t->page); return 1;
    case LJ_VK_NEXT:  change(w, t->pos + t->page); return 1;
    case LJ_VK_HOME:  change(w, t->min); return 1;
    case LJ_VK_END:   change(w, t->max); return 1;
    default: return 0;
    }
}

static const lj_widget_vt tb_vt = { tb_paint, tb_mouse, tb_key, NULL, NULL };

lj_widget *lj_w_trackbar_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_trackbar *t = lj_w_alloc(sizeof *t, parent, d, &tb_vt, 1);
    t->max = 10; t->freq = 1; t->page = 2;
    if (d) {
        t->min = d->min; t->max = d->max > d->min ? d->max : 10; t->pos = d->position;
        t->freq = d->frequency > 0 ? d->frequency : 1;
        t->page = d->page_size > 0 ? d->page_size : 2;
        t->vertical = d->orientation && strcmp(d->orientation, "trVertical") == 0;
        t->ticks_none = d->tick_style && strcmp(d->tick_style, "tsNone") == 0;
        t->ticks_both = d->tick_marks && strcmp(d->tick_marks, "tmBoth") == 0;   /* ponytail: drawn as tmBottomRight until a form needs it */
    }
    lj_w_trackbar_set(&t->base, t->pos);
    return &t->base;
}
