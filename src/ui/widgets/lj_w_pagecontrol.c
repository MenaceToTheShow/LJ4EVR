/* lj_w_pagecontrol.c - TPageControl: the classic raised tab strip (white left/top,
 * black right with #808080 inner, 2-px rounded corners, the active tab 2 px
 * taller and wider and breaking the page frame's top line) over a raised page
 * frame; the active TTabSheet child is shown inset LJ_TAB_PAD, the others hidden.
 * Also the shared paint/mouse/key/layout for TTabControl (tabs from Tabs.Strings). */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>

#define TAB_X0  2             /* first tab's left edge */
#define TAB_PADX 6            /* text padding each side */

int lj_w_tabs_count(const lj_widget *w)
{
    const lj_w_tabs *t = (const lj_w_tabs *)w;
    const lj_widget *c;
    int n = 0;
    if (w->kind == LJ_K_TABCONTROL) return t->ntabs;
    for (c = w->first; c; c = c->next) if (c->kind == LJ_K_TABSHEET) n++;
    return n;
}

lj_widget *lj_w_tabs_page(const lj_widget *w, int i)
{
    lj_widget *c;
    if (w->kind == LJ_K_TABCONTROL || i < 0) return NULL;
    for (c = w->first; c; c = c->next) if (c->kind == LJ_K_TABSHEET && i-- == 0) return c;
    return NULL;
}

static const char *tab_caption(const lj_widget *w, int i)
{
    const lj_w_tabs *t = (const lj_w_tabs *)w;
    const lj_widget *p;
    if (w->kind == LJ_K_TABCONTROL) return i >= 0 && i < t->ntabs ? t->tabs[i] : "";
    p = lj_w_tabs_page(w, i);
    return p && ((const lj_w_tabsheet *)p)->caption ? ((const lj_w_tabsheet *)p)->caption : "";
}

static int tab_width(const lj_widget *w, int i) { return lj_text_width(lj_w_font_of(w), tab_caption(w, i)) + 2 * TAB_PADX; }

int lj_w_tabs_at(const lj_widget *w, int x, int y)
{
    const lj_w_tabs *t = (const lj_w_tabs *)w;
    int i, n = lj_w_tabs_count(w), tx = TAB_X0;
    if (y < 0 || y >= t->tab_h) return -1;
    for (i = 0; i < n; i++) { int tw = tab_width(w, i); if (x >= tx && x < tx + tw) return i; tx += tw; }
    return -1;
}

void lj_w_tabs_layout(lj_widget *w)
{
    lj_w_tabs *t = (lj_w_tabs *)w;
    lj_widget *c;
    int i = 0;
    if (w->kind == LJ_K_TABCONTROL) return;
    if (t->active < 0 && lj_w_tabs_count(w) > 0) {               /* ActivePage = TabSheet2, resolved once the children exist */
        const char *want = w->desc ? w->desc->active_page : NULL;
        t->active = 0;
        for (c = w->first; c; c = c->next) {
            if (c->kind != LJ_K_TABSHEET) continue;
            if (want && c->name && strcmp(c->name, want) == 0) { t->active = i; break; }
            i++;
        }
        i = 0;
    }
    for (c = w->first; c; c = c->next) {
        if (c->kind != LJ_K_TABSHEET) continue;
        c->x = LJ_TAB_PAD; c->y = t->tab_h + LJ_TAB_PAD;
        c->w = w->w - 2 * LJ_TAB_PAD; c->h = w->h - t->tab_h - 2 * LJ_TAB_PAD;
        if (c->w < 0) c->w = 0;
        if (c->h < 0) c->h = 0;
        c->visible = (i == t->active) ? 1u : 0u;
        i++;
    }
}

static void tab_shape(lj_surface *s, int x, int y, int w, int h)
{
    lj_fill(s, x + 1, y + 1, w - 2, h - 1, LJ_CLR_BTNFACE);
    lj_vline(s, x, y + 2, h - 2, LJ_CLR_BTNHIGHLIGHT);
    lj_hline(s, x + 2, y, w - 4, LJ_CLR_BTNHIGHLIGHT);
    lj_fill(s, x + 1, y + 1, 1, 1, LJ_CLR_BTNHIGHLIGHT);
    lj_vline(s, x + w - 1, y + 2, h - 2, LJ_CLR_3DDKSHADOW);
    lj_fill(s, x + w - 2, y + 1, 1, 1, LJ_CLR_3DDKSHADOW);
    lj_vline(s, x + w - 2, y + 2, h - 2, LJ_CLR_BTNSHADOW);
}

void lj_w_tabs_paint(lj_widget *w, lj_surface *s)
{
    lj_w_tabs *t = (lj_w_tabs *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, i, n = lj_w_tabs_count(w), tx = TAB_X0, fy;
    lj_widget_abs(w, &ax, &ay);
    fy = ay + t->tab_h;
    lj_fill(s, ax, ay, w->w, w->h, LJ_CLR_BTNFACE);
    /* page frame: raised */
    lj_bevel1(s, ax, fy, w->w, w->h - t->tab_h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_3DDKSHADOW);
    lj_hline(s, ax + 1, ay + w->h - 2, w->w - 2, LJ_CLR_BTNSHADOW);
    lj_vline(s, ax + w->w - 2, fy + 1, w->h - t->tab_h - 2, LJ_CLR_BTNSHADOW);
    for (i = 0; i < n; i++) {
        int tw = tab_width(w, i), act = i == t->active, x = ax + tx, ty, tt;
        if (act) { x -= 2; tw += 4; ty = ay; tt = t->tab_h + 1; }
        else { ty = ay + 2; tt = t->tab_h - 2; }
        tab_shape(s, x, ty, tw, tt);
        if (act) lj_hline(s, x + 1, fy, tw - 2, LJ_CLR_BTNFACE);   /* the frame's top line breaks under the active tab */
        lj_w_text(s, w, x + (tw - lj_text_width(f, tab_caption(w, i))) / 2, ty + (tt - f->height) / 2 + (act ? 0 : 1), tab_caption(w, i), LJ_CLR_3DDKSHADOW);
        if (act && w->focused) lj_focus_rect(s, x + 3, ty + 3, tw - 6, tt - 5);
        tx += tab_width(w, i);
    }
}

int lj_w_tabs_select(lj_widget *w, int i)
{
    lj_w_tabs *t = (lj_w_tabs *)w;
    if (i < 0 || i >= lj_w_tabs_count(w)) return -1;
    if (i == t->active) return 0;
    lj_w_emit(w, LJ_WEV_CHANGING);
    t->active = i;
    lj_w_tabs_layout(w);
    lj_widget_layout(w);
    lj_widget_invalidate(w);
    lj_w_emit(w, LJ_WEV_CHANGE);
    return 0;
}

int lj_w_tabs_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int i;
    if (!w->enabled || m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return 0;
    i = lj_w_tabs_at(w, m->x, m->y);
    if (i < 0) return 0;
    lj_widget_set_focus(w);
    lj_w_tabs_select(w, i);
    return 1;
}

int lj_w_tabs_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_tabs *t = (lj_w_tabs *)w;
    int n = lj_w_tabs_count(w);
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled || n == 0) return 0;
    switch (k->key) {
    case LJ_VK_LEFT:  lj_w_tabs_select(w, t->active > 0 ? t->active - 1 : 0); return 1;
    case LJ_VK_RIGHT: lj_w_tabs_select(w, t->active + 1 < n ? t->active + 1 : n - 1); return 1;
    case LJ_VK_HOME:  lj_w_tabs_select(w, 0); return 1;
    case LJ_VK_END:   lj_w_tabs_select(w, n - 1); return 1;
    default: return 0;
    }
}

static const lj_widget_vt pc_vt = { lj_w_tabs_paint, lj_w_tabs_mouse, lj_w_tabs_key, lj_w_tabs_layout };

lj_widget *lj_w_pagecontrol_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_tabs *t = lj_w_alloc(sizeof *t, parent, d, &pc_vt, 1);
    t->tab_h = lj_w_font_of(&t->base)->height + 8;    /* 21 for MS Sans Serif 13; rig item */
    t->active = d && d->active_page ? -1 : (d && d->tab_index > 0 ? d->tab_index : 0);
    return &t->base;
}
