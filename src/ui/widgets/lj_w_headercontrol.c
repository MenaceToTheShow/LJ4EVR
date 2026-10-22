/* lj_w_headercontrol.c - THeaderControl: a row of raised sections (face, white
 * top/left, black bottom/right, #808080 inner, design 7.3) whose dividers drag;
 * sections come from desc->columns (caption = Text, width). The paint is shared
 * with the TListView report header. */
#include "ui/widgets/lj_widgets_lists.h"

#define HC_GRAB 3

int lj_w_header_section_at(const int *widths, int n, int x)
{
    int i, sx = 0;
    for (i = 0; i < n; i++) { if (x >= sx && x < sx + widths[i]) return i; sx += widths[i]; }
    return -1;
}

int lj_w_header_divider_at(const int *widths, int n, int x)
{
    int i, sx = 0;
    for (i = 0; i < n; i++) { sx += widths[i]; if (x >= sx - HC_GRAB && x <= sx + HC_GRAB) return i; }
    return -1;
}

void lj_w_header_paint(lj_surface *s, const lj_widget *w, int ax, int ay, int width, int h, const lj_column_desc *cols, const int *widths, int n, int pressed)
{
    const lj_font *f = lj_w_font_of(w);
    int i, x = ax, ty = ay + (h - f->height) / 2;
    for (i = 0; i < n && x < ax + width; i++) {
        int cw = x + widths[i] > ax + width ? ax + width - x : widths[i], tx = x + 4, tw;   /* the last section stops at the view's edge */
        if (i == pressed) { lj_fill(s, x, ay, cw, h, LJ_CLR_BTNFACE); lj_bevel1(s, x, ay, cw, h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT); }
        else lj_button_raised(s, x, ay, cw, h);
        if (cols[i].caption) {
            tw = lj_text_width(f, cols[i].caption);
            if (cols[i].alignment == LJ_TA_RIGHT) tx = x + cw - 4 - tw;
            else if (cols[i].alignment == LJ_TA_CENTER) tx = x + (cw - tw) / 2;
            if (tw + 4 <= cw)   /* ponytail: no per-section clip, a caption wider than its section is dropped */
                lj_w_text(s, w, tx + (i == pressed), ty + (i == pressed), cols[i].caption, LJ_CLR_3DDKSHADOW);
        }
        x += cw;
    }
    if (x < ax + width) lj_button_raised(s, x, ay, ax + width - x, h);   /* the empty remainder is one raised strip */
}

static void hc_paint(lj_widget *w, lj_surface *s)
{
    lj_w_headercontrol *h = (lj_w_headercontrol *)w;
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_w_header_paint(s, w, ax, ay, w->w, w->h, h->sections, h->widths, h->n, h->pressed);
}

static int hc_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_headercontrol *h = (lj_w_headercontrol *)w;
    int sec;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1) return 0;
        sec = lj_w_header_divider_at(h->widths, h->n, m->x);
        if (sec >= 0) { h->drag = sec + 1; h->drag_x = m->x - h->widths[sec]; return 1; }
        sec = lj_w_header_section_at(h->widths, h->n, m->x);
        if (sec >= 0) { h->pressed = sec; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (h->drag) { h->widths[h->drag - 1] = m->x - h->drag_x > 0 ? m->x - h->drag_x : 0; lj_widget_invalidate(w); }
        return h->drag != 0;
    case LJ_EV_MOUSE_UP:
        if (h->drag) { h->clicked = h->drag - 1; h->drag = 0; lj_w_emit(w, LJ_WEV_SECTION_RESIZE); return 1; }
        if (h->pressed >= 0) {
            sec = lj_w_header_section_at(h->widths, h->n, m->x);
            if (sec == h->pressed) { h->clicked = sec; lj_w_emit(w, LJ_WEV_SECTION_CLICK); }
            h->pressed = -1;
            lj_widget_invalidate(w);
            return 1;
        }
        return 0;
    default: return 0;
    }
}

static const lj_widget_vt hc_vt = { hc_paint, hc_mouse, NULL, NULL };

lj_widget *lj_w_headercontrol_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_headercontrol *h = lj_w_alloc(sizeof *h, parent, d, &hc_vt, 0);
    int i;
    h->pressed = -1; h->clicked = -1;
    if (d && d->columns) {
        h->sections = d->columns;
        h->n = d->ncolumns < LJ_LV_COLS ? d->ncolumns : LJ_LV_COLS;
        for (i = 0; i < h->n; i++) h->widths[i] = d->columns[i].width;
    }
    return &h->base;
}
