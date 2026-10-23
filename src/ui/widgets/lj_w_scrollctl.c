/* lj_w_scrollctl.c - scroll control: four raised buttons and the tile well (design 7.2, 7.3; goboscroll.gif). */
#include "ui/widgets/lj_w_scrollctl.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

enum { LJ_SC_BTN_W = 29, LJ_SC_BTN_H = 30, LJ_SC_BTN_Y = 6, LJ_SC_WELL_X = 60, LJ_SC_WELL_W = 45, LJ_SC_WELL_Y = 2, LJ_SC_WELL_H = 37 };   /* goboscroll.gif 132x33: buttons 23x24 at y 5, well (48,2) 36x30, x1.25 (ME-67), rig item */
static const int lj_sc_bx[4] = { 2, 31, 105, 134 };
static const int lj_sc_step[4] = { -2, -1, 1, 2 };

static void glyph(lj_surface *s, int bx, int by, int i, uint32_t rgb)   /* ◄  |◄  ►|  ► */
{
    int cy = by + LJ_SC_BTN_H / 2;
    switch (i) {
    case 0: lj_w_tri(s, bx + 11, cy, 4, 2, rgb); break;
    case 1: lj_vline(s, bx + 9, cy - 3, 7, rgb); lj_w_tri(s, bx + 12, cy, 4, 2, rgb); break;
    case 2: lj_w_tri(s, bx + 16, cy, 4, 3, rgb); lj_vline(s, bx + 19, cy - 3, 7, rgb); break;
    default: lj_w_tri(s, bx + 17, cy, 4, 3, rgb); break;
    }
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_scrollctl *c = (lj_w_scrollctl *)w;
    int ax, ay, i;
    uint32_t ink = w->enabled ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW;
    lj_widget_abs(w, &ax, &ay);
    lj_button_raised(s, ax, ay, w->w, w->h);
    for (i = 0; i < 4; i++) {
        int p = c->pressed == (unsigned)(i + 1);
        lj_w_button(s, ax + lj_sc_bx[i], ay + LJ_SC_BTN_Y, LJ_SC_BTN_W, LJ_SC_BTN_H, LJ_CLR_BTNFACE, p);
        glyph(s, ax + lj_sc_bx[i] + p, ay + LJ_SC_BTN_Y + p, i, ink);
    }
    lj_fill(s, ax + LJ_SC_WELL_X, ay + LJ_SC_WELL_Y, LJ_SC_WELL_W, LJ_SC_WELL_H, LJ_CLR_3DDKSHADOW);
    if (c->tile) lj_blit_rgb(s, ax + LJ_SC_WELL_X + (LJ_SC_WELL_W - c->tile->w) / 2, ay + LJ_SC_WELL_Y + (LJ_SC_WELL_H - c->tile->h) / 2, c->tile, 0);
    else lj_fill(s, ax + LJ_SC_WELL_X + 2, ay + LJ_SC_WELL_Y + 1, LJ_SC_WELL_W - 4, LJ_SC_WELL_H - 2, c->swatch_rgb);
}

static int button_at(int x, int y)
{
    int i;
    if (y < LJ_SC_BTN_Y || y >= LJ_SC_BTN_Y + LJ_SC_BTN_H) return -1;
    for (i = 0; i < 4; i++)
        if (x >= lj_sc_bx[i] && x < lj_sc_bx[i] + LJ_SC_BTN_W) return i;
    return -1;
}

static void step(lj_w_scrollctl *c, int i)
{
    int v = c->value + (lj_sc_step[i] == -2 ? -c->big : lj_sc_step[i] == 2 ? c->big : lj_sc_step[i]);
    if (v < 0) v = 0;
    if (v > c->max) v = c->max;
    c->last = lj_sc_step[i];
    if (v == c->value) return;
    c->value = v;
    lj_widget_invalidate(&c->base);
    lj_w_emit(&c->base, LJ_WEV_CHANGE);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_scrollctl *c = (lj_w_scrollctl *)w;
    int i;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1) return 0;
        i = button_at(m->x, m->y);
        if (i < 0) return lj_w_inside(w, m->x, m->y);
        c->pressed = (unsigned)(i + 1);
        lj_widget_invalidate(w);
        return 1;
    case LJ_EV_MOUSE_UP:
        if (!c->pressed) return 0;
        i = (int)c->pressed - 1;
        c->pressed = 0;
        lj_widget_invalidate(w);
        if (button_at(m->x, m->y) == i) step(c, i);
        return 1;
    case LJ_EV_MOUSE_LEAVE:
        if (c->pressed) { c->pressed = 0; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        if (m->wheel) step(c, m->wheel > 0 ? 2 : 1);
        return 1;
    default:
        return 0;
    }
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_scrollctl *c = (lj_w_scrollctl *)w;
    if (!w->enabled || k->type != LJ_EV_KEY_DOWN) return 0;
    switch (k->key) {
    case LJ_VK_LEFT:  step(c, k->ctrl ? 0 : 1); return 1;
    case LJ_VK_RIGHT: step(c, k->ctrl ? 3 : 2); return 1;
    default: return 0;
    }
}

static const lj_widget_vt vt = { paint, mouse, key, NULL, NULL };

lj_widget *lj_w_scrollctl_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_scrollctl *c = lj_w_alloc(sizeof *c, parent, d, &vt, 1);
    c->max = 255; c->big = 16; c->swatch_rgb = LJ_CLR_BTNHIGHLIGHT;
    if (d) {
        if (d->max > 0) c->max = d->max;
        if (d->step > 0) c->big = d->step;
        c->value = d->position < 0 ? 0 : d->position > c->max ? c->max : d->position;
    } else {
        c->base.w = LJ_SCROLLCTL_W; c->base.h = LJ_SCROLLCTL_H;
    }
    return &c->base;
}

int lj_w_scrollctl_value(const lj_widget *w) { return ((const lj_w_scrollctl *)w)->value; }
int lj_w_scrollctl_last(const lj_widget *w)  { return ((const lj_w_scrollctl *)w)->last; }

int lj_w_scrollctl_set_value(lj_widget *w, int v)
{
    lj_w_scrollctl *c = (lj_w_scrollctl *)w;
    c->value = v < 0 ? 0 : v > c->max ? c->max : v;
    lj_widget_invalidate(w);
    return 0;
}

void lj_w_scrollctl_set_tile(lj_widget *w, const lj_surface *tile, uint32_t swatch_rgb)
{
    lj_w_scrollctl *c = (lj_w_scrollctl *)w;
    c->tile = tile; c->swatch_rgb = swatch_rgb & 0xFFFFFFu;
    lj_widget_invalidate(w);
}
