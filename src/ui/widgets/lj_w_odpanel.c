/* lj_w_odpanel.c - TODPanel (ME-17): Martin's captioned list host; its DFM child sits
 * at Top 18, so a 1-px lowered frame and a 17-px caption band. The band's look is
 * unverified (no screenshot shows one): face, caption centred, etched bottom line. */
#include "ui/widgets/lj_w_odpanel.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

#define LJ_ODPANEL_BAND 17

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    int x, y;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, w->color);
    lj_bevel1(s, x, y, w->w, w->h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
    lj_hline(s, x + 1, y + LJ_ODPANEL_BAND - 1, w->w - 2, LJ_CLR_BTNSHADOW);
    lj_hline(s, x + 1, y + LJ_ODPANEL_BAND, w->w - 2, LJ_CLR_BTNHIGHLIGHT);
    lj_w_caption_text(s, w, x + (w->w - lj_text_width(f, c->caption)) / 2, y + 1 + (LJ_ODPANEL_BAND - 2 - f->height) / 2, c->caption);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static void client(const lj_widget *w, lj_rect *r) { r->x = 1; r->y = LJ_ODPANEL_BAND + 1; r->w = w->w - 2; r->h = w->h - LJ_ODPANEL_BAND - 2; }   /* ME-43: under the band (child at Top 18) */

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, client };

lj_widget *lj_w_odpanel_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
}
