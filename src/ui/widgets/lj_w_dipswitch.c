/* lj_w_dipswitch.c - TDipSwitch: black 1-px frame, blue base, white arrow, ten 4x10 switch
 * slots with a 4-row knob, 3x5 digits (design 7.2; geometry measured from showdipswitch.gif,
 * switch columns 8,13,18,23,27,32,36,41,46,51 as Martin's component spaces them). */
#include "ui/widgets/lj_w_dipswitch.h"

#include <stdlib.h>
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

#define LJ_DIP_MAX 10
static const int lj_dip_x[LJ_DIP_MAX] = { 8, 13, 18, 23, 27, 32, 36, 41, 46, 51 };
enum { LJ_DIP_SW_W = 4, LJ_DIP_SW_Y = 4, LJ_DIP_SW_H = 10, LJ_DIP_KNOB = 4, LJ_DIP_DIGIT_Y = 16 };

/* 3x5 digits, our own pixels: rows top to bottom, 3 bits MSB left. */
static const uint8_t lj_dip_digit[10][5] = {
    { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 }, { 5, 5, 7, 1, 1 },
    { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
};

static void digit(lj_surface *s, int x, int y, int d, uint32_t rgb)
{
    int r, c;
    for (r = 0; r < 5; r++)
        for (c = 0; c < 3; c++)
            if (lj_dip_digit[d][r] & (4 >> c)) lj_fill(s, x + c, y + r, 1, 1, rgb);
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_dipswitch *ds = (lj_w_dipswitch *)w;
    int ax, ay, i;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, LJ_CLR_3DDKSHADOW);
    lj_fill(s, ax + 1, ay + 1, w->w - 2, w->h - 2, ds->base_rgb);
    lj_vline(s, ax + 4, ay + 4, 11, ds->dip_rgb);                            /* the "down = on" arrow */
    lj_w_tri(s, ax + 4, ay + 15, 2, 1, ds->dip_rgb);
    for (i = 0; i < ds->nswitches; i++) {
        int x = ax + lj_dip_x[i], on = (ds->value >> i) & 1;
        lj_fill(s, x, ay + LJ_DIP_SW_Y, LJ_DIP_SW_W, LJ_DIP_SW_H, LJ_CLR_3DDKSHADOW);
        lj_fill(s, x, ay + LJ_DIP_SW_Y + (on ? LJ_DIP_SW_H - LJ_DIP_KNOB : 0), LJ_DIP_SW_W, LJ_DIP_KNOB, ds->dip_rgb);
        digit(s, ax + 6 + 5 * i, ay + LJ_DIP_DIGIT_Y, (i + 1) % 10, ds->text_rgb);
    }
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_dipswitch *ds = (lj_w_dipswitch *)w;
    int i;
    if (!w->enabled || m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return 0;
    for (i = 0; i < ds->nswitches; i++) {
        if (m->x >= lj_dip_x[i] && m->x < lj_dip_x[i] + LJ_DIP_SW_W + 1 && m->y >= LJ_DIP_SW_Y - 1 && m->y <= LJ_DIP_SW_Y + LJ_DIP_SW_H) {
            ds->value ^= 1 << i;
            lj_widget_invalidate(w);
            lj_w_emit(w, LJ_WEV_CHANGE);
            return 1;
        }
    }
    return 1;
}

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_dipswitch_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_dipswitch *ds = lj_w_alloc(sizeof *ds, parent, d, &vt, 0);
    ds->nswitches = LJ_DIP_MAX;
    ds->base_rgb = 0x0000FFu; ds->dip_rgb = LJ_CLR_BTNHIGHLIGHT; ds->text_rgb = LJ_CLR_BTNHIGHLIGHT;   /* clBlue / clWhite / clWhite, the three DFMs */
    if (d) {
        if (d->kind_str && d->kind_str[0] == 'T' && d->kind_str[1] == 'D') {
            int n = atoi(d->kind_str + 4);                                   /* "TDip10" */
            if (n >= 1 && n <= LJ_DIP_MAX) ds->nswitches = n;
        }
        ds->base_rgb = lj_desc_rgb(d->base_color, ds->base_rgb);
        ds->dip_rgb = lj_desc_rgb(d->dip_color, ds->dip_rgb);
        ds->text_rgb = lj_desc_rgb(d->text_color, ds->text_rgb);
        ds->value = d->value & ((1 << ds->nswitches) - 1);
    }
    return &ds->base;
}

int lj_w_dipswitch_value(const lj_widget *w) { return ((const lj_w_dipswitch *)w)->value; }

int lj_w_dipswitch_set_value(lj_widget *w, int v)
{
    lj_w_dipswitch *ds = (lj_w_dipswitch *)w;
    ds->value = v & ((1 << ds->nswitches) - 1);
    lj_widget_invalidate(w);
    return 0;
}
