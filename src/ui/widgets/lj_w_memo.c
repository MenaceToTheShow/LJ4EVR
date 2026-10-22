/* lj_w_memo.c - TMemo, read-only plain text (design 7.2): sunken clWindow box,
 * one line per Lines.Strings entry at the font pitch, vertical scrollbar when
 * ScrollBars = ssVertical/ssBoth (always shown, as the VCL does) and wheel/key
 * scrolling. No wrapping (ponytail: WordWrap ignored, add when a note overflows).
 * Also the shared vt for TRichEdit. */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>
#include "ui/widgets/lj_widgets_basic.h"

#define MM_BORDER 2
#define MM_PADX   3
#define MM_PADY   1

static int inner_h(const lj_w_memo *m) { return m->base.h - 2 * (m->border ? MM_BORDER : 0) - 2 * MM_PADY; }
static int visible_lines(const lj_w_memo *m) { int v = inner_h(m) / m->line_h; return v < 1 ? 1 : v; }

static void fit_sb(lj_w_memo *m)
{
    int vis = visible_lines(m), b = m->border ? MM_BORDER : 0;
    m->sb.vertical = 1;
    m->sb.x = m->base.w - b - LJ_SB_W; m->sb.y = b; m->sb.w = LJ_SB_W; m->sb.h = m->base.h - 2 * b;
    m->sb.min = 0; m->sb.max = m->n - vis > 0 ? m->n - vis : 0; m->sb.page = vis;
    if (!m->vscroll) m->sb.max = 0;
    lj_w_sbar_set(&m->sb, m->sb.pos);
}

void lj_w_memo_paint(lj_widget *w, lj_surface *s)
{
    lj_w_memo *m = (lj_w_memo *)w;
    int ax, ay, i, vis, b = m->border ? MM_BORDER : 0, avail = w->w - 2 * b - 2 * MM_PADX - (m->vscroll ? LJ_SB_W : 0);
    char line[256];
    lj_widget_abs(w, &ax, &ay);
    if (m->border) lj_edge_sunken(s, ax, ay, w->w, w->h, w->color);
    else lj_fill(s, ax, ay, w->w, w->h, w->color);
    fit_sb(m);
    vis = visible_lines(m);
    for (i = 0; i < vis && m->sb.pos + i < m->n; i++) {   /* the desc's Font.Color (ME-89: About is white on black), cut at the last glyph that fits left of the scrollbar */
        size_t n = strlen(m->lines[m->sb.pos + i]);
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, m->lines[m->sb.pos + i], n); line[n] = 0;
        while (n && lj_text_width(lj_w_font_of(w), line) > avail) line[--n] = 0;
        lj_w_text(s, w, ax + b + MM_PADX, ay + b + MM_PADY + i * m->line_h, line, lj_w_fg(w));
    }
    if (m->vscroll) lj_w_sbar_paint(s, &m->sb, ax, ay, w->enabled);
}

int lj_w_memo_mouse(lj_widget *w, const lj_mouse_ev *mo)
{
    lj_w_memo *m = (lj_w_memo *)w;
    int r;
    if (!w->enabled) return 0;
    fit_sb(m);
    if (mo->type == LJ_EV_MOUSE_WHEEL) { if (lj_w_sbar_set(&m->sb, m->sb.pos - 3 * mo->wheel)) lj_widget_invalidate(w); return 1; }
    if (m->vscroll) {
        r = lj_w_sbar_mouse(&m->sb, mo);
        if (r == 1) lj_widget_invalidate(w);
        if (r >= 0) return 1;
    }
    if (mo->type == LJ_EV_MOUSE_DOWN && mo->button == 1) { lj_widget_set_focus(w); return 1; }
    return 0;
}

int lj_w_memo_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_memo *m = (lj_w_memo *)w;
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled) return 0;
    fit_sb(m);
    if (lj_w_sbar_key(&m->sb, k->key)) lj_widget_invalidate(w);
    return k->key == LJ_VK_UP || k->key == LJ_VK_DOWN || k->key == LJ_VK_PRIOR || k->key == LJ_VK_NEXT || k->key == LJ_VK_HOME || k->key == LJ_VK_END;
}

static const lj_widget_vt memo_vt = { lj_w_memo_paint, lj_w_memo_mouse, lj_w_memo_key, NULL };

lj_widget *lj_w_memo_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_memo *m = lj_w_alloc(sizeof *m, parent, d, &memo_vt, 1);
    m->border = 1;
    m->base.color = LJ_CLR_BTNHIGHLIGHT;
    if (d) {
        m->base.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        m->lines = d->items; m->n = d->items ? d->nitems : 0;
        m->vscroll = d->scroll_bars && (strcmp(d->scroll_bars, "ssVertical") == 0 || strcmp(d->scroll_bars, "ssBoth") == 0);
        m->border = !(d->border_style && strcmp(d->border_style, "bsNone") == 0);
    }
    m->line_h = lj_w_font_of(&m->base)->height;
    return &m->base;
}

void lj_w_memo_set_lines(lj_widget *w, const char *const *lines, int n)
{
    lj_w_memo *m = (lj_w_memo *)w;
    m->lines = lines; m->n = lines ? n : 0; m->sb.pos = 0;
    lj_widget_invalidate(w);
}
