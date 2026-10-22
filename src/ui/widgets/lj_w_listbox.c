/* lj_w_listbox.c - TListBox: sunken clWindow box, 13-px rows, navy #000080 selection
 * with white text, embedded vertical scrollbar when the items overflow (design 7.3).
 * Also the shared row paint/mouse/key used by TCheckListBox and the TComboBox drop list. */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>

#define LB_BORDER 2
#define LB_CHECK  13           /* TCheckListBox square, 7.3 */

static int inner_h(const lj_w_list *l) { return l->base.h - 2 * LB_BORDER; }
static int visible_rows(const lj_w_list *l) { int v = inner_h(l) / (l->item_h > 0 ? l->item_h : 13); return v < 1 ? 1 : v; }
static int needs_sb(const lj_w_list *l) { return l->n * l->item_h > inner_h(l); }

#define FIT(l) lj_w_sbar_fit(&(l)->sb, (l)->base.w, LB_BORDER, LB_BORDER, inner_h(l), (l)->n, visible_rows(l))

int lj_w_list_row_at(const lj_widget *w, int y)
{
    const lj_w_list *l = (const lj_w_list *)w;
    int row;
    if (y < LB_BORDER || y >= w->h - LB_BORDER || l->item_h <= 0) return -1;
    row = l->sb.pos + (y - LB_BORDER) / l->item_h;
    return row < l->n ? row : -1;
}

static void check_glyph(lj_surface *s, int x, int y)
{
    /* the classic 7x7 check: columns 0-1 rise, 2-6 fall, 3 px thick */
    int i;
    for (i = 0; i < 7; i++) {
        int dy = i < 2 ? 2 + i : 6 - i;
        lj_vline(s, x + i, y + dy, 3, LJ_CLR_3DDKSHADOW);
    }
}

void lj_w_list_paint(lj_widget *w, lj_surface *s)
{
    lj_w_list *l = (lj_w_list *)w;
    int ax, ay, i, vis, sb, text_x, row_w;
    const lj_font *f = lj_w_font_of(w);
    lj_widget_abs(w, &ax, &ay);
    if (l->flat) lj_fill(s, ax, ay, w->w, w->h, w->color);               /* BorderStyle = bsNone */
    else if (l->owner) { lj_fill(s, ax, ay, w->w, w->h, w->color); lj_bevel1(s, ax, ay, w->w, w->h, LJ_CLR_3DDKSHADOW, LJ_CLR_3DDKSHADOW); }   /* combo drop list: 1-px black frame */
    else lj_edge_sunken(s, ax, ay, w->w, w->h, w->color);
    FIT(l);
    sb = needs_sb(l);
    vis = visible_rows(l);
    row_w = w->w - 2 * LB_BORDER - (sb ? LJ_SB_W : 0);
    text_x = ax + LB_BORDER + (l->checks_on ? LB_CHECK + 7 : 2);
    for (i = 0; i < vis && l->sb.pos + i < l->n; i++) {
        int item = l->sb.pos + i, ry = ay + LB_BORDER + i * l->item_h;
        int sel = item == l->index;
        uint32_t ink = sel ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW;
        if (ry + l->item_h > ay + w->h - LB_BORDER) break;
        if (sel) lj_fill(s, ax + LB_BORDER, ry, row_w, l->item_h, LJ_CLR_HIGHLIGHT);
        if (l->checks_on) {
            int bx = ax + LB_BORDER + 1, by = ry + (l->item_h - LB_CHECK) / 2;
            lj_edge_sunken(s, bx, by, LB_CHECK, LB_CHECK, w->enabled ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_BTNFACE);
            if (lj_w_list_checked(w, item)) check_glyph(s, bx + 3, by + 3);
        }
        if (!w->enabled) lj_w_text(s, w, text_x, ry + (l->item_h - f->height) / 2, l->items[item], ink);
        else lj_text(s, f, text_x, ry + (l->item_h - f->height) / 2, l->items[item], ink, 0);
        if (sel && w->focused && !l->owner) lj_focus_rect(s, ax + LB_BORDER, ry, row_w, l->item_h);
    }
    if (sb) lj_w_sbar_paint(s, &l->sb, ax, ay, w->enabled);
}

int lj_w_list_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_list *l = (lj_w_list *)w;
    int row;
    if (!w->enabled) return 0;
    FIT(l);
    if (m->type == LJ_EV_MOUSE_WHEEL) {
        if (lj_w_sbar_set(&l->sb, l->sb.pos - 3 * m->wheel)) lj_widget_invalidate(w);
        return 1;
    }
    if (needs_sb(l) || l->sb.drag) {
        int r = lj_w_sbar_mouse(&l->sb, m);
        if (r == 1) lj_widget_invalidate(w);
        if (r >= 0) return 1;
    }
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return m->type == LJ_EV_MOUSE_DOWN;
    if (!l->owner) lj_widget_set_focus(w);
    row = lj_w_list_row_at(w, m->y);
    if (row >= 0 && row != l->index) { l->index = row; lj_widget_invalidate(w); lj_w_emit(w, LJ_WEV_CLICK); }
    if (row >= 0 && l->checks_on && m->x >= LB_BORDER && m->x < LB_BORDER + LB_CHECK + 2) {
        lj_w_list_set_checked(w, row, !lj_w_list_checked(w, row));
        lj_w_emit(w, LJ_WEV_CLICK_CHECK);
    }
    if (row >= 0 && m->clicks == 2) lj_w_emit(w, LJ_WEV_DBLCLICK);
    return 1;
}

int lj_w_list_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_list *l = (lj_w_list *)w;
    int next = l->index, vis = visible_rows(l);
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled || l->n == 0) return 0;
    switch (k->key) {
    case LJ_VK_UP:    next = l->index > 0 ? l->index - 1 : 0; break;
    case LJ_VK_DOWN:  next = l->index + 1 < l->n ? l->index + 1 : l->n - 1; break;
    case LJ_VK_PRIOR: next = l->index - vis > 0 ? l->index - vis : 0; break;
    case LJ_VK_NEXT:  next = l->index + vis < l->n ? l->index + vis : l->n - 1; break;
    case LJ_VK_HOME:  next = 0; break;
    case LJ_VK_END:   next = l->n - 1; break;
    case LJ_VK_SPACE:
        if (!l->checks_on || l->index < 0) return 0;
        lj_w_list_set_checked(w, l->index, !lj_w_list_checked(w, l->index));
        lj_w_emit(w, LJ_WEV_CLICK_CHECK);
        return 1;
    default: return 0;
    }
    if (next != l->index) {
        l->index = next;
        FIT(l); lj_w_sbar_reveal(&l->sb, l->index, visible_rows(l));
        lj_widget_invalidate(w);
        lj_w_emit(w, LJ_WEV_CLICK);
    }
    return 1;
}

void lj_w_list_set_items(lj_widget *w, const char *const *items, int n)
{
    lj_w_list *l = (lj_w_list *)w;
    l->items = items; l->n = items ? n : 0;
    l->index = -1; l->sb.pos = 0;
    memset(l->checks, 0, sizeof l->checks);
    lj_widget_invalidate(w);
}

int lj_w_list_checked(const lj_widget *w, int i)
{
    const lj_w_list *l = (const lj_w_list *)w;
    if (i < 0 || i >= (int)sizeof l->checks * 8) return 0;
    return (l->checks[i >> 3] >> (i & 7)) & 1;
}

void lj_w_list_set_checked(lj_widget *w, int i, int on)
{
    lj_w_list *l = (lj_w_list *)w;
    if (i < 0 || i >= (int)sizeof l->checks * 8) return;
    if (on) l->checks[i >> 3] |= (uint8_t)(1u << (i & 7));
    else l->checks[i >> 3] &= (uint8_t)~(1u << (i & 7));
    lj_widget_invalidate(w);
}

static const lj_widget_vt lb_vt = { lj_w_list_paint, lj_w_list_mouse, lj_w_list_key, NULL };

lj_widget *lj_w_listbox_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_list *l = lj_w_alloc(sizeof *l, parent, d, &lb_vt, 1);
    l->index = -1;
    l->item_h = 13;
    if (d) {
        l->base.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        l->items = d->items; l->n = d->items ? d->nitems : 0;
        if (d->item_height > 0) l->item_h = d->item_height;
        if (d->item_index >= 0 && d->item_index < l->n) l->index = d->item_index;
        l->flat = d->border_style && strcmp(d->border_style, "bsNone") == 0;
    }
    return &l->base;
}
