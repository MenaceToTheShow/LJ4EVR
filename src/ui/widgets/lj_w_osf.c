/* lj_w_osf.c - O/S/F button: raised coloured face, caption, left/right click cycle (design 7.2, 7.3, D10, ME-27). */
#include "ui/widgets/lj_w_osf.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static const uint32_t lj_osf_rgb[3] = { LJ_CLR_RED, LJ_CLR_YELLOW, LJ_CLR_GREEN };   /* off.gif / snap.gif / fade.gif sampled [07 fact-check 7] */
static const char *const lj_osf_caption[3] = { "Off", "Snap", "Fade" };

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_osf *b = (lj_w_osf *)w;
    int ax, ay, p = b->pressed;
    lj_widget_abs(w, &ax, &ay);
    lj_w_button(s, ax, ay, w->w, w->h, lj_osf_rgb[b->state], p);
    lj_w_text_c(s, w, ax + p, ay + p, w->w, w->h, lj_osf_caption[b->state], LJ_CLR_3DDKSHADOW, 0);
    if (w->focused) lj_focus_rect(s, ax + 4, ay + 4, w->w - 8, w->h - 8);
}

static void cycle(lj_w_osf *b, int dir)
{
    int i, st;
    for (i = 1; i <= 3; i++) {
        st = (b->state + dir * i + 3) % 3;
        if (b->allowed & (1u << st)) { b->state = st; break; }
    }
    lj_widget_invalidate(&b->base);
    lj_w_emit(&b->base, LJ_WEV_CLICK);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_osf *b = (lj_w_osf *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if ((m->button != 1 && m->button != 2) || !lj_w_inside(w, m->x, m->y)) return 0;
        b->pressed = 1; b->press_button = m->button;
        lj_widget_invalidate(w);
        lj_w_emit(w, LJ_WEV_MOUSE_DOWN);
        return 1;
    case LJ_EV_MOUSE_UP:
        if (!b->pressed) return 0;
        b->pressed = 0;
        lj_widget_invalidate(w);
        lj_w_emit(w, LJ_WEV_MOUSE_UP);
        if (lj_w_inside(w, m->x, m->y)) cycle(b, b->press_button == 2 ? -1 : 1);
        return 1;
    case LJ_EV_MOUSE_LEAVE:
        if (b->pressed) { b->pressed = 0; lj_widget_invalidate(w); }
        return 1;
    default:
        return 0;
    }
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    if (!w->enabled || k->type != LJ_EV_KEY_DOWN || k->key != LJ_VK_SPACE) return 0;
    cycle((lj_w_osf *)w, 1);
    return 1;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL };

lj_widget *lj_w_osf_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_osf *b = lj_w_alloc(sizeof *b, parent, d, &vt, 1);
    b->allowed = LJ_OSF_ALLOW_ALL;
    if (d && d->position >= LJ_OSF_OFF && d->position <= LJ_OSF_FADE) b->state = d->position;
    if (!d) { b->base.w = 48; b->base.h = 24; }
    return &b->base;
}

int lj_w_osf_state(const lj_widget *w) { return ((const lj_w_osf *)w)->state; }

int lj_w_osf_set_state(lj_widget *w, int state)
{
    lj_w_osf *b = (lj_w_osf *)w;
    if (state < LJ_OSF_OFF || state > LJ_OSF_FADE || !(b->allowed & (1u << state))) return -1;
    b->state = state;
    lj_widget_invalidate(w);
    return 0;
}

void lj_w_osf_set_allowed(lj_widget *w, unsigned mask)
{
    lj_w_osf *b = (lj_w_osf *)w;
    b->allowed = (mask & LJ_OSF_ALLOW_ALL) ? (mask & LJ_OSF_ALLOW_ALL) : LJ_OSF_ALLOW_ALL;
    if (!(b->allowed & (1u << b->state))) { int i; for (i = 0; i < 3; i++) if (b->allowed & (1u << i)) { b->state = i; break; } }
    lj_widget_invalidate(w);
}
