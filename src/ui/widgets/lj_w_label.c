/* lj_w_label.c - TLabel: caption text, Alignment, Layout, Transparent, FocusControl (7.3, 7.9). */
#include "ui/widgets/lj_w_label.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "gfx/lj_draw.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, tx, ty, tw;
    lj_widget_abs(w, &ax, &ay);
    if (!w->transparent) lj_fill(s, ax, ay, w->w, w->h, w->color);
    tw = lj_text_width(f, c->caption);
    tx = ax; ty = ay;
    if (w->desc) {
        if (w->desc->alignment == LJ_TA_RIGHT) tx = ax + w->w - tw;
        else if (w->desc->alignment == LJ_TA_CENTER) tx = ax + (w->w - tw) / 2;
        if (w->desc->layout && strcmp(w->desc->layout, "tlCenter") == 0) ty = ay + (w->h - f->height) / 2;
        else if (w->desc->layout && strcmp(w->desc->layout, "tlBottom") == 0) ty = ay + w->h - f->height;
    }
    lj_w_caption_text(s, w, tx, ty, c->caption);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_label_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
}
