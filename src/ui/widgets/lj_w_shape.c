/* lj_w_shape.c - TShape: Shape stRectangle (default) / stCircle / stEllipse / stSquare,
 * Brush.Color (default clWhite) fill, Pen.Color (default black) outline Pen.Width
 * thick. Ellipses are integer: pixel centres inside the ellipse of the box. */
#include "ui/widgets/lj_w_shape.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "gfx/lj_draw.h"

static int in_ellipse(int i, int j, int w, int h)
{
    int64_t dx = 2 * (int64_t)i + 1 - w, dy = 2 * (int64_t)j + 1 - h;
    return dx * dx * h * h + dy * dy * w * w <= (int64_t)w * w * h * h;
}

static void ellipse(lj_surface *s, int x, int y, int w, int h, uint32_t brush, uint32_t pen, int pw)
{
    int i, j, di, dj;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            int edge = 0;
            if (!in_ellipse(i, j, w, h)) continue;
            for (dj = -pw; dj <= pw && !edge; dj++)
                for (di = -pw; di <= pw && !edge; di++)
                    if (di * di + dj * dj <= pw * pw && !in_ellipse(i + di, j + dj, w, h)) edge = 1;
            lj_fill(s, x + i, y + j, 1, 1, edge ? pen : brush);
        }
}

static void paint(lj_widget *w, lj_surface *s)
{
    const lj_ctl_desc *d = w->desc;
    const char *shape = d && d->shape ? d->shape : "stRectangle";
    uint32_t brush = d ? lj_desc_rgb(d->brush_color, LJ_CLR_BTNHIGHLIGHT) : LJ_CLR_BTNHIGHLIGHT;
    uint32_t pen = d ? lj_desc_rgb(d->pen_color, LJ_CLR_3DDKSHADOW) : LJ_CLR_3DDKSHADOW;
    int pw = d && d->pen_width > 0 ? d->pen_width : 1, i;
    int x, y, ww = w->w, hh = w->h;
    lj_widget_abs(w, &x, &y);
    if (strcmp(shape, "stCircle") == 0 || strcmp(shape, "stSquare") == 0) {   /* the largest centred square */
        int sz = ww < hh ? ww : hh;
        x += (ww - sz) / 2; y += (hh - sz) / 2; ww = sz; hh = sz;
    }
    if (strcmp(shape, "stCircle") == 0 || strcmp(shape, "stEllipse") == 0) { ellipse(s, x, y, ww, hh, brush, pen, pw); return; }
    lj_fill(s, x, y, ww, hh, brush);
    for (i = 0; i < pw; i++) lj_bevel1(s, x + i, y + i, ww - 2 * i, hh - 2 * i, pen, pen);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_shape_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_alloc(sizeof(lj_widget), parent, d, &vt, 0);
}
