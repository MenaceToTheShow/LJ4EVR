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

#define TAB_MAX 64            /* tabs per strip (the DFMs peak at 11) */
typedef struct { int x, y, w; } tab_pos;   /* inactive tab rect in widget coords (y = row top, height row_h + 2) */

static int row_h(const lj_widget *w) { return lj_w_font_of(w)->height + 6; }   /* 19 for MS Sans Serif 13; the strip is rows * row_h + 2 */

/* Tab geometry. One row, or with MultiLine the comctl32 TCS_MULTILINE rule: tabs
 * wrap greedily, every row is justified to the full width, and the active tab's row
 * rotates to the bottom (against the page). Fills p[0..n-1], returns the row count. */
static int tab_rows(const lj_widget *w, tab_pos *p, int n)
{
    const lj_w_tabs *t = (const lj_w_tabs *)w;
    int ml = w->desc ? lj_desc_bool(w->desc->multiline, 0) : 0, avail = w->w - 2 * TAB_X0;
    int i, r, rows = 1, x = 0, rh = row_h(w), first[TAB_MAX + 1], rowof[TAB_MAX];
    if (n > TAB_MAX) n = TAB_MAX;
    first[0] = 0;
    for (i = 0; i < n; i++) {
        int tw = tab_width(w, i);
        if (ml && x > 0 && x + tw > avail) { first[rows++] = i; x = 0; }
        p[i].x = x; p[i].w = tw; rowof[i] = rows - 1; x += tw;
    }
    first[rows] = n;
    if (ml && rows > 1) {
        int act;
        for (r = 1; r < rows; r++) first[r] = first[r - 1] + n / rows + (r - 1 >= rows - n % rows);   /* comctl32 balances: equal counts, the extras on the last rows */
        for (r = 0; r < rows; r++) for (i = first[r]; i < first[r + 1]; i++) rowof[i] = r;
        act = t->active >= 0 && t->active < n ? rowof[t->active] : rows - 1;
        for (r = 0; r < rows; r++) {
            int cnt = first[r + 1] - first[r], used = 0, extra, k;
            for (k = first[r]; k < first[r + 1]; k++) used += p[k].w;
            extra = avail - used > 0 ? avail - used : 0; x = 0;
            for (k = first[r]; k < first[r + 1]; k++) { p[k].x = x; p[k].w += extra / cnt + (k - first[r] < extra % cnt); x += p[k].w; }
        }
        for (i = 0; i < n; i++) rowof[i] = (rowof[i] - act - 1 + rows) % rows;   /* act -> rows - 1, cyclic order kept */
    }
    for (i = 0; i < n; i++) { p[i].x += TAB_X0; p[i].y = rowof[i] * rh; }
    return rows;
}

int lj_w_tabs_at(const lj_widget *w, int x, int y)
{
    tab_pos p[TAB_MAX];
    int i, n = lj_w_tabs_count(w), rh = row_h(w);
    if (n > TAB_MAX) n = TAB_MAX;
    tab_rows(w, p, n);
    for (i = 0; i < n; i++) if (x >= p[i].x && x < p[i].x + p[i].w && y >= p[i].y && y < p[i].y + rh + 2) return i;
    return -1;
}

void lj_w_tabs_layout(lj_widget *w)
{
    lj_w_tabs *t = (lj_w_tabs *)w;
    lj_widget *c;
    tab_pos p[TAB_MAX];
    int i = 0, n = lj_w_tabs_count(w);
    if (t->active < 0 && w->kind != LJ_K_TABCONTROL && n > 0) {               /* ActivePage = TabSheet2, resolved once the children exist */
        const char *want = w->desc ? w->desc->active_page : NULL;
        t->active = 0;
        for (c = w->first; c; c = c->next) {
            if (c->kind != LJ_K_TABSHEET) continue;
            if (want && c->name && strcmp(c->name, want) == 0) { t->active = i; break; }
            i++;
        }
        i = 0;
    }
    if (w->kind == LJ_K_TABCONTROL) {                                         /* AdjustClientRect = DisplayRect: an alClient child sits under the strip (one row) */
        if (n > 0) t->tab_h = tab_rows(w, p, n > TAB_MAX ? TAB_MAX : n) * row_h(w) + 2;   /* tabs set at run time keep the created height until they exist */
        for (c = w->first; c; c = c->next) {                                  /* tc1 of TFixtureSetupForm: 404x269, ODPanel21 at 4,24 396x241 (DFM) */
            if (c->align != LJ_AL_CLIENT) continue;
            c->x = LJ_TAB_PAD; c->y = lj_w_font_of(w)->height + 8 + 3;
            c->w = w->w - 2 * LJ_TAB_PAD; c->h = w->h - c->y - LJ_TAB_PAD;
            if (c->w < 0) c->w = 0;
            if (c->h < 0) c->h = 0;
        }
        return;
    }
    t->tab_h = tab_rows(w, p, n > TAB_MAX ? TAB_MAX : n) * row_h(w) + 2;
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

static void tab_text(lj_surface *s, const lj_widget *w, int x, int y, const char *str, uint32_t rgb)   /* '&' prefixes as the tab control draws them: "&&" = '&' */
{
    const lj_font *f = lj_w_font_of(w);
    if (w->enabled) { lj_text(s, f, x, y, str, rgb, 1); return; }
    lj_text_disabled(s, f, x, y, str, 1);
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
    tab_pos p[TAB_MAX];
    int ax, ay, i, n = lj_w_tabs_count(w), fy, rh = row_h(w);
    if (n > TAB_MAX) n = TAB_MAX;
    t->tab_h = tab_rows(w, p, n) * rh + 2;
    lj_widget_abs(w, &ax, &ay);
    fy = ay + t->tab_h;
    lj_fill(s, ax, ay, w->w, w->h, LJ_CLR_BTNFACE);
    /* page frame: raised */
    lj_bevel1(s, ax, fy, w->w, w->h - t->tab_h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_3DDKSHADOW);
    lj_hline(s, ax + 1, ay + w->h - 2, w->w - 2, LJ_CLR_BTNSHADOW);
    lj_vline(s, ax + w->w - 2, fy + 1, w->h - t->tab_h - 2, LJ_CLR_BTNSHADOW);
    for (i = 0; i < n; i++) {                                     /* upper rows first so the bottom row overlaps them */
        int tw = p[i].w, act = i == t->active, x = ax + p[i].x, ty, tt;
        if (act) continue;
        ty = ay + p[i].y + 2; tt = rh;
        tab_shape(s, x, ty, tw, tt);
        tab_text(s, w, x + (tw - lj_text_width(f, tab_caption(w, i))) / 2, ty + (tt - f->height) / 2 + 1, tab_caption(w, i), LJ_CLR_3DDKSHADOW);
    }
    if (t->active >= 0 && t->active < n) {
        int tw = p[t->active].w + 4, x = ax + p[t->active].x - 2, ty = ay + p[t->active].y, tt = rh + 3;
        tab_shape(s, x, ty, tw, tt);
        lj_hline(s, x + 1, fy, tw - 2, LJ_CLR_BTNFACE);   /* the frame's top line breaks under the active tab */
        tab_text(s, w, x + (tw - lj_text_width(f, tab_caption(w, t->active))) / 2, ty + (tt - f->height) / 2, tab_caption(w, t->active), LJ_CLR_3DDKSHADOW);
        if (w->focused) lj_focus_rect(s, x + 3, ty + 3, tw - 6, tt - 5);
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
