/* lj_w_panel.c - TPanel as TCustomPanel.Paint draws it: bsSingle black frame, BevelOuter
 * (default bvRaised), BorderWidth, BevelInner (default bvNone), BevelWidth nested 1-px
 * bevels, face fill, caption centred per Alignment (7.3). Children are the window's. */
#include "ui/widgets/lj_w_panel.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "gfx/lj_draw.h"

static void bevel(lj_surface *s, lj_rect *r, lj_bevel kind, int width, uint32_t face)
{
    int i;
    uint32_t tl = face, br = face;
    if (kind == LJ_BV_RAISED) { tl = LJ_CLR_BTNHIGHLIGHT; br = LJ_CLR_BTNSHADOW; }
    else if (kind == LJ_BV_LOWERED) { tl = LJ_CLR_BTNSHADOW; br = LJ_CLR_BTNHIGHLIGHT; }
    for (i = 0; i < width && r->w > 0 && r->h > 0; i++) {
        lj_bevel1(s, r->x, r->y, r->w, r->h, tl, br);
        r->x++; r->y++; r->w -= 2; r->h -= 2;
    }
}

void lj_w_panel_paint_frame(lj_widget *w, lj_surface *s, lj_rect *r)
{
    const lj_ctl_desc *d = w->desc;
    int bw = d && d->bevel_width > 0 ? d->bevel_width : 1;
    lj_bevel outer = d && d->bevel_outer ? d->bevel_outer : LJ_BV_RAISED;
    lj_bevel inner = d && d->bevel_inner ? d->bevel_inner : LJ_BV_NONE;
    lj_widget_abs(w, &r->x, &r->y);
    r->w = w->w; r->h = w->h;
    lj_fill(s, r->x, r->y, r->w, r->h, w->color);
    if (d && d->border_style && strcmp(d->border_style, "bsSingle") == 0) {
        lj_bevel1(s, r->x, r->y, r->w, r->h, LJ_CLR_3DDKSHADOW, LJ_CLR_3DDKSHADOW);
        r->x++; r->y++; r->w -= 2; r->h -= 2;
    }
    if (outer != LJ_BV_NONE) bevel(s, r, outer, bw, w->color);
    if (d && d->border_width > 0) { r->x += d->border_width; r->y += d->border_width; r->w -= 2 * d->border_width; r->h -= 2 * d->border_width; }
    if (inner != LJ_BV_NONE) bevel(s, r, inner, bw, w->color);
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    lj_rect r;
    int tx, tw;
    lj_w_panel_paint_frame(w, s, &r);
    if (!c->caption[0]) return;
    tw = lj_text_width(f, c->caption);
    tx = r.x + (r.w - tw) / 2;
    if (w->desc && w->desc->alignment == LJ_TA_LEFT) tx = r.x;
    else if (w->desc && w->desc->alignment == LJ_TA_RIGHT) tx = r.x + r.w - tw;
    lj_w_caption_text(s, w, tx, r.y + (r.h - f->height) / 2, c->caption);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

lj_widget *lj_w_panel_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
}
