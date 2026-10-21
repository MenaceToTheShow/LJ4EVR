/* lj_w_statusbar.c - TStatusBar: clBtnFace bar of sunken panels (#808080 top/left,
 * white bottom/right, 2-px gaps, the last panel stretching to the edge as the
 * VCL does), text at +3, and the classic sizing grip (design 7.7: 17/19-px bar).
 * Panels come from desc->columns (caption = Text, width); SimplePanel = one. */
#include "ui/widgets/lj_widgets_lists.h"

#define SB_GAP 2
#define SB_TOP 2

static void grip(lj_surface *s, int x, int y)                /* 3 diagonal #808080 ridges with a white edge, ending at the bottom-right corner (x,y) */
{
    int i, k;
    for (k = 2; k <= 10; k += 4)
        for (i = 0; i <= k; i++) {
            lj_fill(s, x - k + i, y - i, 1, 1, LJ_CLR_BTNSHADOW);
            lj_fill(s, x - k + i - 1, y - i, 1, 1, LJ_CLR_BTNHIGHLIGHT);
        }
}

static void sb_paint(lj_widget *w, lj_surface *s)
{
    lj_w_statusbar *b = (lj_w_statusbar *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, i, x, n = b->simple ? 1 : b->n, ph = w->h - SB_TOP - 1, ty;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
    x = ax;
    ty = ay + SB_TOP + (ph - f->height) / 2;
    for (i = 0; i < n; i++) {
        int pw = b->simple || i == n - 1 ? ax + w->w - x - (b->grip ? 14 : 0) : b->panels[i].width;
        const char *t = b->text[i] ? b->text[i] : (b->simple ? NULL : b->panels[i].caption);
        if (pw <= 0) break;
        lj_bevel1(s, x, ay + SB_TOP, pw, ph, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
        if (t && lj_text_width(f, t) + 6 <= pw) lj_w_text(s, w, x + 3, ty, t, LJ_CLR_3DDKSHADOW);
        x += pw + SB_GAP;
    }
    if (b->grip) grip(s, ax + w->w - 2, ay + w->h - 2);
}

static const lj_widget_vt sb_vt = { sb_paint, NULL, NULL, NULL };

lj_widget *lj_w_statusbar_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_statusbar *b = lj_w_alloc(sizeof *b, parent, d, &sb_vt, 0);
    b->grip = 1;
    if (d) {
        b->panels = d->columns; b->n = d->columns ? (d->ncolumns < LJ_SB_PANELS ? d->ncolumns : LJ_SB_PANELS) : 0;
        b->simple = lj_desc_bool(d->simple_panel, 0) != 0 || b->n == 0;
        b->text[0] = d->text ? d->text : (b->simple ? d->caption : NULL);
        if (!d->align) b->base.align = LJ_AL_BOTTOM;              /* the VCL default for TStatusBar */
    }
    return &b->base;
}

int lj_w_statusbar_set_text(lj_widget *w, int panel, const char *text)
{
    lj_w_statusbar *b = (lj_w_statusbar *)w;
    if (panel < 0 || panel >= LJ_SB_PANELS) return -1;
    b->text[panel] = text;
    lj_widget_invalidate(w);
    return 0;
}
