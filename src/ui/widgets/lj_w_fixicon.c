/* lj_w_fixicon.c - fixture icon tile, paint only (design 7.8, 7.3). */
#include "ui/widgets/lj_w_fixicon.h"

#include <stdio.h>
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static const uint32_t lj_fix_cap_rgb[4] = { 0x0000FFu, 0xFF0000u, 0x800000u, 0x808080u };   /* CHM 2387-2399, 1037; iconsondesktop.gif #0000ff */

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_fixicon *f = (lj_w_fixicon *)w;
    char cap[96];
    int ax, ay, bh = w->h - LJ_FIXICON_CAP_H - 2;
    lj_widget_abs(w, &ax, &ay);
    lj_button_raised(s, ax, ay, w->w, w->h);
    lj_fill(s, ax + 1, ay, w->w - 3, LJ_FIXICON_CAP_H, lj_fix_cap_rgb[f->state & 3]);
    snprintf(cap, sizeof cap, "(%d) %s", f->number, f->user_name ? f->user_name : "");
    lj_w_text_fit(s, w, ax + 2, ay + 1, cap, LJ_CLR_BTNHIGHLIGHT, w->w - 5);
    if (f->bitmap && bh > 0)
        lj_blit_rgb(s, ax + 1 + (w->w - 3 - f->bitmap->w) / 2, ay + LJ_FIXICON_CAP_H + (bh - f->bitmap->h) / 2, f->bitmap, 1);
    if (f->programmed) lj_fill(s, ax + 3, ay + LJ_FIXICON_CAP_H + 3, 5, 5, f->programmed == LJ_FIX_PROG_SCENE ? LJ_CLR_GREEN : 0x00FFFFu);
}

static const lj_widget_vt vt = { paint, NULL, NULL, NULL, NULL };

lj_widget *lj_w_fixicon_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_fixicon *f = lj_w_alloc(sizeof *f, parent, d, &vt, 0);
    if (d) { f->number = d->tag; f->user_name = d->caption; }
    else { f->base.w = LJ_FIXICON_W; f->base.h = LJ_FIXICON_H; }
    return &f->base;
}

void lj_w_fixicon_set(lj_widget *w, int state, int programmed, const lj_surface *bitmap)
{
    lj_w_fixicon *f = (lj_w_fixicon *)w;
    f->state = state & 3; f->programmed = programmed; f->bitmap = bitmap;
    lj_widget_invalidate(w);
}
