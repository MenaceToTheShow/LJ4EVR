/* lj_w_led.c - LED button: raised face, ringed dot, caption; click on release (design 7.2, 7.3). */
#include "ui/widgets/lj_w_led.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

enum { LJ_LED_CX = 13, LJ_LED_CY = 12, LJ_LED_TEXT_X = 25 };
static const int lj_led_hw[7] = { 5, 5, 5, 4, 3, 2, -1 };      /* green half-width per |dy|; -1 = the 5-px cap row (measured) */

static void dot(lj_surface *s, int cx, int cy, uint32_t rgb)
{
    int dy;
    for (dy = -6; dy <= 6; dy++) {
        int g = lj_led_hw[dy < 0 ? -dy : dy];
        if (g < 0) { lj_hline(s, cx - 2, cy + dy, 5, LJ_CLR_3DDKSHADOW); continue; }
        lj_hline(s, cx - g, cy + dy, 2 * g + 1, rgb);
        lj_fill(s, cx - g - 1, cy + dy, 1, 1, LJ_CLR_3DDKSHADOW);
        lj_fill(s, cx + g + 1, cy + dy, 1, 1, LJ_CLR_3DDKSHADOW);
    }
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_led *b = (lj_w_led *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, p = b->pressed;
    lj_widget_abs(w, &ax, &ay);
    lj_w_button(s, ax, ay, w->w, w->h, w->color, p);
    dot(s, ax + LJ_LED_CX + p, ay + LJ_LED_CY + p, b->on ? LJ_LED_ON_RGB : LJ_LED_OFF_RGB);
    if (b->caption) {
        int ty = ay + (w->h - f->height) / 2 + p;
        if (w->enabled) lj_text(s, f, ax + LJ_LED_TEXT_X + p, ty, b->caption, LJ_CLR_3DDKSHADOW, 1);
        else lj_text_disabled(s, f, ax + LJ_LED_TEXT_X + p, ty, b->caption, 1);
    }
    if (w->focused) lj_focus_rect(s, ax + 4, ay + 4, w->w - 8, w->h - 8);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_led *b = (lj_w_led *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
        b->pressed = 1;
        lj_widget_invalidate(w);
        lj_w_emit(w, LJ_WEV_MOUSE_DOWN);
        return 1;
    case LJ_EV_MOUSE_UP:
        if (!b->pressed) return 0;
        b->pressed = 0;
        lj_widget_invalidate(w);
        lj_w_emit(w, LJ_WEV_MOUSE_UP);
        if (lj_w_inside(w, m->x, m->y)) lj_w_emit(w, LJ_WEV_CLICK);
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
    lj_w_emit(w, LJ_WEV_CLICK);
    return 1;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL };

lj_widget *lj_w_led_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_led *b = lj_w_alloc(sizeof *b, parent, d, &vt, 1);
    if (d) { b->caption = d->caption; b->on = lj_desc_bool(d->checked, 0) ? 1u : 0u; }
    return &b->base;
}

void lj_w_led_set(lj_widget *w, int on)
{
    ((lj_w_led *)w)->on = on ? 1u : 0u;
    lj_widget_invalidate(w);
}

int lj_w_led_on(const lj_widget *w) { return ((const lj_w_led *)w)->on; }
