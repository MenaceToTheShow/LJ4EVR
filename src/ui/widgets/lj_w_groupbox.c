/* lj_w_groupbox.c - TGroupBox as TCustomGroupBox.Paint draws it: face, white frame at
 * (1, H/2) to the far edge over a #808080 frame at (0, H/2-1) one pixel short (the
 * etched look), caption at x 8 in an opaque face cell breaking the top line (7.3). */
#include "ui/widgets/lj_w_groupbox.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

void lj_w_groupbox_paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    int x, y, top = f->height / 2;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, w->color);
    lj_bevel1(s, x + 1, y + top, w->w - 1, w->h - top, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNHIGHLIGHT);
    lj_bevel1(s, x, y + top - 1, w->w - 1, w->h - top, LJ_CLR_BTNSHADOW, LJ_CLR_BTNSHADOW);
    if (c->caption[0]) {
        lj_fill(s, x + 8, y, lj_text_width(f, c->caption), f->height, w->color);
        lj_w_caption_text(s, w, x + 8, y, c->caption);
    }
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { lj_w_groupbox_paint, mouse, NULL, NULL };

lj_widget *lj_w_groupbox_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
}
