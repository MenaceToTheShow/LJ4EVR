/* lj_w_statictext.c - TStaticText: an opaque windowed label, text at the top, Alignment (7.3). */
#include "ui/widgets/lj_w_statictext.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    int ax, ay, tx, tw = lj_text_width(lj_w_font_of(w), c->caption);
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
    tx = ax;
    if (w->desc && w->desc->alignment == LJ_TA_RIGHT) tx = ax + w->w - tw;
    else if (w->desc && w->desc->alignment == LJ_TA_CENTER) tx = ax + (w->w - tw) / 2;
    lj_w_caption_text(s, w, tx, ay, c->caption);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_statictext_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
}
