/* lj_w_treeview.c - TTreeView (design 7.2 "minimal tree variant"): sunken clWindow
 * box, 16-px rows, Indent 19, 9x9 +/- boxes (#808080 frame, white fill) and
 * dotted #808080 lines as the classic look; items[] carry one leading '\t' per
 * level (the generator's decoding of Items.Data). Navy selection, arrows/+/- keys. */
#include "ui/widgets/lj_widgets_lists.h"

#define TV_BORDER 2
#define TV_BOX    9

static int level_of(const char *s) { int l = 0; while (*s == '\t') { l++; s++; } return l; }
static const char *text_of(const char *s) { while (*s == '\t') s++; return s; }

int lj_w_treeview_level(const lj_widget *w, int i)
{
    const lj_w_treeview *t = (const lj_w_treeview *)w;
    return i >= 0 && i < t->n ? level_of(t->items[i]) : 0;
}

int lj_w_treeview_expanded(const lj_widget *w, int i)
{
    const lj_w_treeview *t = (const lj_w_treeview *)w;
    if (i < 0 || i >= (int)sizeof t->expanded * 8) return 0;
    return (t->expanded[i >> 3] >> (i & 7)) & 1;
}

void lj_w_treeview_expand(lj_widget *w, int i, int on)
{
    lj_w_treeview *t = (lj_w_treeview *)w;
    if (i < 0 || i >= (int)sizeof t->expanded * 8) return;
    if (on) t->expanded[i >> 3] |= (uint8_t)(1u << (i & 7));
    else t->expanded[i >> 3] &= (uint8_t)~(1u << (i & 7));
    lj_widget_invalidate(w);
}

static int has_children(const lj_w_treeview *t, int i) { return i + 1 < t->n && level_of(t->items[i + 1]) > level_of(t->items[i]); }

static int has_next_sibling(const lj_w_treeview *t, int i)
{
    int j, l = level_of(t->items[i]);
    for (j = i + 1; j < t->n; j++) {
        int lj = level_of(t->items[j]);
        if (lj < l) return 0;
        if (lj == l) return 1;
    }
    return 0;
}

/* ponytail: O(n) walk per lookup, fine for profile trees of a few hundred nodes; cache the visible list if it shows */
static int walk(const lj_w_treeview *t, int want_row)
{
    int i, row = 0, hide = -1;
    for (i = 0; i < t->n; i++) {
        int l = level_of(t->items[i]);
        if (hide >= 0 && l > hide) continue;
        hide = -1;
        if (row == want_row) return i;
        row++;
        if (has_children(t, i) && !lj_w_treeview_expanded(&t->base, i)) hide = l;
    }
    return want_row < 0 ? row : -1;
}
int lj_w_treeview_visible_count(const lj_widget *w) { return walk((const lj_w_treeview *)w, -1); }
int lj_w_treeview_visible_item(const lj_widget *w, int row) { return row < 0 ? -1 : walk((const lj_w_treeview *)w, row); }

static int row_of_item(const lj_w_treeview *t, int item)
{
    int r, n = walk(t, -1);
    for (r = 0; r < n; r++) if (walk(t, r) == item) return r;
    return -1;
}

static int inner_h(const lj_w_treeview *t) { return t->base.h - 2 * TV_BORDER; }
static int visible_rows(const lj_w_treeview *t) { int v = inner_h(t) / t->row_h; return v < 1 ? 1 : v; }
static int total_rows(const lj_w_treeview *t) { return walk(t, -1); }
static int needs_sb(const lj_w_treeview *t) { return total_rows(t) * t->row_h > inner_h(t); }

#define FIT(t) lj_w_sbar_fit(&(t)->sb, (t)->base.w, TV_BORDER, TV_BORDER, inner_h(t), total_rows(t), visible_rows(t))

static int node_x(const lj_w_treeview *t, int level) { return TV_BORDER + (level + (t->show_root ? 0 : -1)) * t->indent + t->indent / 2; }   /* centre of the button column */
static int text_x(const lj_w_treeview *t, int level) { return node_x(t, level) + t->indent / 2 + 2; }

static void dotted_h(lj_surface *s, int x, int y, int w) { int i; for (i = 0; i < w; i++) if (((x + i) ^ y) & 1) lj_fill(s, x + i, y, 1, 1, LJ_CLR_BTNSHADOW); }
static void dotted_v(lj_surface *s, int x, int y, int h) { int i; for (i = 0; i < h; i++) if (((y + i) ^ x) & 1) lj_fill(s, x, y + i, 1, 1, LJ_CLR_BTNSHADOW); }

static void tv_paint(lj_widget *w, lj_surface *s)
{
    lj_w_treeview *t = (lj_w_treeview *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, r, vis, sb;
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, w->color);
    FIT(t);
    sb = needs_sb(t);
    vis = visible_rows(t);
    for (r = 0; r < vis; r++) {
        int item = walk(t, t->sb.pos + r), l, ry, cy, nx, tx, tw, a, p;
        const char *txt;
        if (item < 0) break;
        l = level_of(t->items[item]);
        ry = ay + TV_BORDER + r * t->row_h; cy = ry + t->row_h / 2;
        nx = ax + node_x(t, l); tx = ax + text_x(t, l);
        txt = text_of(t->items[item]);
        tw = lj_text_width(f, txt);
        if (t->show_lines) {
            if (l > 0 || t->show_root) {
                dotted_h(s, nx, cy, tx - nx - 2);
                if (!(l == 0 && item == 0)) dotted_v(s, nx, ry, cy - ry);
                if (has_next_sibling(t, item)) dotted_v(s, nx, cy, ry + t->row_h - cy);
            }
            for (a = l - 1, p = item; a >= 0; a--) {        /* ancestor chains that continue below this row */
                while (p > 0 && level_of(t->items[p]) > a) p--;
                if ((a > 0 || t->show_root) && has_next_sibling(t, p)) dotted_v(s, ax + node_x(t, a), ry, t->row_h);
            }
        }
        if (t->show_buttons && has_children(t, item) && (l > 0 || t->show_root)) {
            int bx = nx - TV_BOX / 2, by = cy - TV_BOX / 2;
            lj_fill(s, bx, by, TV_BOX, TV_BOX, LJ_CLR_BTNHIGHLIGHT);
            lj_bevel1(s, bx, by, TV_BOX, TV_BOX, LJ_CLR_BTNSHADOW, LJ_CLR_BTNSHADOW);
            lj_hline(s, bx + 2, cy, TV_BOX - 4, LJ_CLR_3DDKSHADOW);
            if (!lj_w_treeview_expanded(w, item)) lj_vline(s, nx, by + 2, TV_BOX - 4, LJ_CLR_3DDKSHADOW);
        }
        if (item == t->index) {
            lj_fill(s, tx - 2, ry, tw + 4, t->row_h, LJ_CLR_HIGHLIGHT);
            if (w->focused) lj_focus_rect(s, tx - 2, ry, tw + 4, t->row_h);
        }
        lj_w_text(s, w, tx, ry + (t->row_h - f->height) / 2, txt, item == t->index ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);
    }
    if (sb) lj_w_sbar_paint(s, &t->sb, ax, ay, w->enabled);
}

static void select_item(lj_w_treeview *t, int item)
{
    if (item < 0 || item >= t->n || item == t->index) return;
    t->index = item;
    FIT(t); lj_w_sbar_reveal(&t->sb, row_of_item(t, t->index), visible_rows(t));
    lj_widget_invalidate(&t->base);
    lj_w_emit(&t->base, LJ_WEV_CHANGE);
}

static void toggle(lj_w_treeview *t, int item)
{
    if (!has_children(t, item)) return;
    lj_w_treeview_expand(&t->base, item, !lj_w_treeview_expanded(&t->base, item));
    FIT(t);
}

static int tv_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_treeview *t = (lj_w_treeview *)w;
    int row, item, l, nx;
    if (!w->enabled) return 0;
    FIT(t);
    if (m->type == LJ_EV_MOUSE_WHEEL) { if (lj_w_sbar_set(&t->sb, t->sb.pos - 3 * m->wheel)) lj_widget_invalidate(w); return 1; }
    if (needs_sb(t) || t->sb.drag) {
        int r = lj_w_sbar_mouse(&t->sb, m);
        if (r == 1) lj_widget_invalidate(w);
        if (r >= 0) return 1;
    }
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return m->type == LJ_EV_MOUSE_DOWN;
    lj_widget_set_focus(w);
    if (m->y < TV_BORDER || m->y >= w->h - TV_BORDER) return 1;
    row = t->sb.pos + (m->y - TV_BORDER) / t->row_h;
    item = walk(t, row);
    if (item < 0) return 1;
    l = level_of(t->items[item]);
    nx = node_x(t, l);
    if (m->x >= nx - TV_BOX / 2 - 1 && m->x <= nx + TV_BOX / 2 + 1 && t->show_buttons) { toggle(t, item); return 1; }
    if (m->x >= text_x(t, l) - 2) {
        select_item(t, item);
        if (m->clicks == 2) { toggle(t, item); lj_w_emit(w, LJ_WEV_DBLCLICK); }
    }
    return 1;
}

static int tv_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_treeview *t = (lj_w_treeview *)w;
    int row = row_of_item(t, t->index), tot = total_rows(t), vis = visible_rows(t), next = -1;
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled || t->n == 0) return 0;
    switch (k->key) {
    case LJ_VK_UP:    next = row > 0 ? row - 1 : 0; break;
    case LJ_VK_DOWN:  next = row + 1 < tot ? row + 1 : tot - 1; break;
    case LJ_VK_PRIOR: next = row - vis > 0 ? row - vis : 0; break;
    case LJ_VK_NEXT:  next = row + vis < tot ? row + vis : tot - 1; break;
    case LJ_VK_HOME:  next = 0; break;
    case LJ_VK_END:   next = tot - 1; break;
    case LJ_VK_ADD:   if (t->index >= 0 && !lj_w_treeview_expanded(w, t->index)) toggle(t, t->index); return 1;
    case LJ_VK_SUBTRACT: if (t->index >= 0 && lj_w_treeview_expanded(w, t->index)) toggle(t, t->index); return 1;
    case LJ_VK_RIGHT:
        if (t->index < 0) return 1;
        if (has_children(t, t->index) && !lj_w_treeview_expanded(w, t->index)) toggle(t, t->index);
        else if (has_children(t, t->index)) select_item(t, t->index + 1);
        return 1;
    case LJ_VK_LEFT:
        if (t->index < 0) return 1;
        if (has_children(t, t->index) && lj_w_treeview_expanded(w, t->index)) toggle(t, t->index);
        else { int p = t->index, l = level_of(t->items[p]); while (p > 0 && level_of(t->items[p]) >= l) p--; if (level_of(t->items[p]) < l) select_item(t, p); }
        return 1;
    default: return 0;
    }
    select_item(t, walk(t, next));
    return 1;
}

static const lj_widget_vt tv_vt = { tv_paint, tv_mouse, tv_key, NULL, NULL };

lj_widget *lj_w_treeview_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_treeview *t = lj_w_alloc(sizeof *t, parent, d, &tv_vt, 1);
    t->index = -1; t->row_h = 16; t->indent = 19;
    t->show_root = 1; t->show_lines = 1; t->show_buttons = 1;
    t->base.color = LJ_CLR_BTNHIGHLIGHT;
    if (d) {
        t->base.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        t->items = d->items; t->n = d->items ? d->nitems : 0;
        if (d->indent > 0) t->indent = d->indent;
        t->show_root = lj_desc_bool(d->show_root, 1) != 0;
        t->show_lines = lj_desc_bool(d->show_lines, 1) != 0;
    }
    return &t->base;
}

void lj_w_treeview_set_items(lj_widget *w, const char *const *items, int n)
{
    lj_w_treeview *t = (lj_w_treeview *)w;
    int i;
    t->items = items; t->n = items ? n : 0;
    t->index = -1; t->sb.pos = 0;
    for (i = 0; i < (int)sizeof t->expanded; i++) t->expanded[i] = 0;
    lj_widget_invalidate(w);
}
