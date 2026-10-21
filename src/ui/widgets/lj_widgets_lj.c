/* lj_widgets_lj.c - registration of the LJ widget group plus the drawing helpers
 * the lj_w_*.c files of the group share (design 7.2, 7.3, 7.6). */
#include "ui/widgets/lj_widgets_lj.h"

#include <string.h>
#include "gfx/lj_draw.h"

int lj_widgets_register_lj(lj_widget_ctor *table, int count)
{
    if (!table || count < LJ_K_COUNT) return -1;
    table[LJ_K_UPDOWN]    = lj_w_updown_create;
    table[LJ_K_UDBUTTON]  = lj_w_udbutton_create;
    table[LJ_K_SPINEDIT]  = lj_w_spinedit_create;
    table[LJ_K_DIPSWITCH] = lj_w_dipswitch_create;
    return 0;
}

int lj_w_inside(const lj_widget *w, int x, int y)
{
    return x >= 0 && y >= 0 && x < w->w && y < w->h;
}

void lj_w_button(lj_surface *s, int x, int y, int w, int h, uint32_t face, int pressed)
{
    if (pressed) { lj_edge_sunken(s, x, y, w, h, face); return; }   /* 7.3 pressed: #808080/white outer, black/#C0C0C0 inner */
    lj_button_raised(s, x, y, w, h);
    if (face != LJ_CLR_BTNFACE) lj_fill(s, x + 1, y + 1, w - 3, h - 3, face);
}

void lj_w_tri(lj_surface *s, int x, int y, int n, int dir, uint32_t rgb)
{
    int i;
    for (i = 0; i < n; i++) {
        switch (dir) {
        case 0:  lj_hline(s, x - i, y + i, 2 * i + 1, rgb); break;
        case 1:  lj_hline(s, x - i, y - i, 2 * i + 1, rgb); break;
        case 2:  lj_vline(s, x + i, y - i, 2 * i + 1, rgb); break;
        default: lj_vline(s, x - i, y - i, 2 * i + 1, rgb); break;
        }
    }
}

/* The classic spin glyph is 5 wide x 3 tall; the pressed half shifts it +1,+1. */
void lj_w_spin_pair(lj_surface *s, const lj_widget *w, int x, int y, int bw, int bh, int pressed)
{
    int h1 = bh / 2, h2 = bh - h1, cx = x + bw / 2, p1 = pressed == 1, p2 = pressed == 2;
    uint32_t ink = w->enabled ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW;
    lj_w_button(s, x, y, bw, h1, LJ_CLR_BTNFACE, p1);
    lj_w_button(s, x, y + h1, bw, h2, LJ_CLR_BTNFACE, p2);
    if (!w->enabled) {                                               /* white shadow +1,+1 under the #808080 glyph (7.3) */
        lj_w_tri(s, cx + 1, y + h1 / 2, 3, 0, LJ_CLR_BTNHIGHLIGHT);
        lj_w_tri(s, cx + 1, y + h1 + h2 / 2 + 2, 3, 1, LJ_CLR_BTNHIGHLIGHT);
    }
    lj_w_tri(s, cx + p1, y + h1 / 2 - 1 + p1, 3, 0, ink);
    lj_w_tri(s, cx + p2, y + h1 + h2 / 2 + 1 + p2, 3, 1, ink);   /* +1: spike A updown() / lj_spinner, the W0 golden reference */
}

static void draw(lj_surface *s, const lj_widget *w, int x, int y, const char *str, uint32_t rgb, int accel)
{
    if (w->enabled) lj_text(s, lj_w_font_of(w), x, y, str, rgb, accel);
    else lj_text_disabled(s, lj_w_font_of(w), x, y, str, accel);
}

void lj_w_text_c(lj_surface *s, const lj_widget *w, int x, int y, int bw, int bh, const char *str, uint32_t rgb, int accel)
{
    const lj_font *f = lj_w_font_of(w);
    if (!str) return;
    draw(s, w, x + (bw - lj_text_width(f, str)) / 2, y + (bh - f->height) / 2, str, rgb, accel);
}

void lj_w_text_fit(lj_surface *s, const lj_widget *w, int x, int y, const char *str, uint32_t rgb, int maxw)
{
    const lj_font *f = lj_w_font_of(w);
    char buf[256];
    size_t n;
    if (!str || maxw <= 0) return;
    n = strlen(str);
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, str, n);
    buf[n] = 0;
    while (n > 0 && lj_text_width(f, buf) > maxw) buf[--n] = 0;
    if (n) draw(s, w, x, y, buf, rgb, 0);
}
