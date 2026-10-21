/* lj_w_fader.c - vertical fader: ▲, gradient track under a blue cover, value text, ▼; drag, wheel, keys (design 7.2, 7.3). */
#include "ui/widgets/lj_w_fader.h"

#include <stdio.h>
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

#define LJ_FADER_BLUE 0x0000FFu

static int track_h(const lj_widget *w) { int h = w->h - LJ_FADER_CHROME; return h < 1 ? 1 : h; }

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_fader *f = (lj_w_fader *)w;
    const lj_font *font = lj_w_font_of(w);
    char buf[8];
    int ax, ay, i, th = track_h(w), covered, p1 = f->pressed == 1, p2 = f->pressed == 2, cx;
    uint32_t ink = w->enabled ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW;
    lj_widget_abs(w, &ax, &ay);
    lj_button_raised(s, ax, ay, w->w, w->h);
    cx = ax + LJ_FADER_TRACK_X + LJ_FADER_TRACK_W / 2;
    lj_w_button(s, ax, ay + 1, w->w - 2, LJ_FADER_BTN_H, LJ_CLR_BTNFACE, p1);
    lj_w_tri(s, cx + p1, ay + 4 + p1, 6, 0, ink);
    lj_w_button(s, ax, ay + w->h - 2 - LJ_FADER_BTN_H, w->w - 2, LJ_FADER_BTN_H, LJ_CLR_BTNFACE, p2);
    lj_w_tri(s, cx + p2, ay + w->h - 2 - 4 + p2, 6, 1, ink);
    for (i = 0; i < th; i++) {                                    /* white at the top to black at the bottom */
        uint32_t g = th > 1 ? (uint32_t)(255 - 255 * i / (th - 1)) : 255u;
        lj_hline(s, ax + LJ_FADER_TRACK_X, ay + LJ_FADER_TRACK_Y + i, LJ_FADER_TRACK_W, (g << 16) | (g << 8) | g);
    }
    covered = th - (f->max > 0 ? (th * f->value + f->max / 2) / f->max : 0);
    lj_fill(s, ax + LJ_FADER_TRACK_X, ay + LJ_FADER_TRACK_Y, LJ_FADER_TRACK_W, covered, LJ_FADER_BLUE);
    snprintf(buf, sizeof buf, "%d%%", f->max > 0 ? (f->value * 100 + f->max / 2) / f->max : 0);
    lj_text(s, font, cx - lj_text_width(font, buf) / 2, ay + LJ_FADER_TRACK_Y + th - 4 - font->height, buf, LJ_CLR_BTNHIGHLIGHT, 0);
}

static int set(lj_w_fader *f, int v)
{
    if (v < 0) v = 0;
    if (v > f->max) v = f->max;
    if (v == f->value) return 0;
    f->value = v;
    lj_widget_invalidate(&f->base);
    lj_w_emit(&f->base, LJ_WEV_CHANGE);
    return 1;
}

static void set_from_y(lj_w_fader *f, int y)
{
    int th = track_h(&f->base), bottom = LJ_FADER_TRACK_Y + th - 1;
    set(f, th > 1 ? ((bottom - y) * f->max + (th - 1) / 2) / (th - 1) : 0);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_fader *f = (lj_w_fader *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
        if (m->y < LJ_FADER_TRACK_Y) { f->pressed = 1; set(f, f->value + 1); }
        else if (m->y >= LJ_FADER_TRACK_Y + track_h(w)) { f->pressed = 2; set(f, f->value - 1); }
        else { f->dragging = 1; set_from_y(f, m->y); }
        lj_widget_invalidate(w);
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (f->dragging) set_from_y(f, m->y);
        return f->dragging;
    case LJ_EV_MOUSE_UP:
    case LJ_EV_MOUSE_LEAVE:
        if (f->pressed || f->dragging) { f->pressed = 0; f->dragging = 0; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        if (m->wheel) set(f, f->value + (m->wheel > 0 ? 1 : -1));
        return 1;
    default:
        return 0;
    }
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_fader *f = (lj_w_fader *)w;
    if (!w->enabled || k->type != LJ_EV_KEY_DOWN) return 0;
    switch (k->key) {
    case LJ_VK_UP:    set(f, f->value + 1); return 1;
    case LJ_VK_DOWN:  set(f, f->value - 1); return 1;
    case LJ_VK_PRIOR: set(f, f->value + 16); return 1;
    case LJ_VK_NEXT:  set(f, f->value - 16); return 1;
    case LJ_VK_HOME:  set(f, f->max); return 1;
    case LJ_VK_END:   set(f, 0); return 1;
    default: return 0;
    }
}

static const lj_widget_vt vt = { paint, mouse, key, NULL };

lj_widget *lj_w_fader_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_fader *f = lj_w_alloc(sizeof *f, parent, d, &vt, 1);
    f->max = 255;
    if (d) {
        if (d->max > 0) f->max = d->max;
        f->value = d->position < 0 ? 0 : d->position > f->max ? f->max : d->position;
    } else {
        f->base.w = LJ_FADER_W; f->base.h = LJ_FADER_CHROME + 128;
    }
    return &f->base;
}

int lj_w_fader_value(const lj_widget *w) { return ((const lj_w_fader *)w)->value; }

int lj_w_fader_set_value(lj_widget *w, int v)
{
    lj_w_fader *f = (lj_w_fader *)w;
    f->value = v < 0 ? 0 : v > f->max ? f->max : v;
    lj_widget_invalidate(w);
    return 0;
}
