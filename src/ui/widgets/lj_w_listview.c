/* lj_w_listview.c - TListView, report view (design 7.2 "minimal report variant"):
 * sunken clWindow box, 17-px raised column header (dividers drag), 14-px rows,
 * sub-items split on '\t', navy #000080 selection with white text (whole row with
 * RowSelect, else the first column's label), HideSelection honoured, optional
 * #C0C0C0 grid lines, vertical scrollbar on overflow, arrow/page/home/end keys. */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>
#include "core/lj_str.h"
#include "gfx/lj_glyphs.h"

#define LV_BORDER 2

/* ME-160: in the vsIcon view a "row" is a line of tiles (icon_cols per line) of tile_h pixels, so the scrollbar, the
 * visible count and the reveal work in tile lines; report rows are row_h pixels, one item each */
static int icon_cols(const lj_w_listview *v) { int c = (v->base.w - 2 * LV_BORDER - LJ_SB_W) / v->tile_w; return c < 1 ? 1 : c; }
static int rows_top(const lj_w_listview *v) { return LV_BORDER + (v->show_headers && !v->tile_w ? v->header_h : 0); }
static int rows_h(const lj_w_listview *v) { return v->base.h - rows_top(v) - LV_BORDER; }
static int pitch(const lj_w_listview *v) { return v->tile_w ? v->tile_h : v->row_h; }
static int lines(const lj_w_listview *v) { return v->tile_w ? (v->n + icon_cols(v) - 1) / icon_cols(v) : v->n; }
static int line_of(const lj_w_listview *v, int item) { return v->tile_w ? item / icon_cols(v) : item; }
static int visible_rows(const lj_w_listview *v) { int n = rows_h(v) / pitch(v); return n < 1 ? 1 : n; }
static int needs_sb(const lj_w_listview *v) { return lines(v) * pitch(v) > rows_h(v); }

#define FIT(v) lj_w_sbar_fit(&(v)->sb, (v)->base.w, LV_BORDER, rows_top(v), rows_h(v), lines(v), visible_rows(v))

static int icon_at(const lj_w_listview *v, int x, int y)   /* the tile under a widget-relative point, -1 = none */
{
    int c = (x - LV_BORDER) / v->tile_w, item;
    if (x < LV_BORDER || y < rows_top(v) || y >= v->base.h - LV_BORDER || c >= icon_cols(v)) return -1;
    item = (v->sb.pos + (y - rows_top(v)) / v->tile_h) * icon_cols(v) + c;
    return item < v->n ? item : -1;
}

static void icon_paint(lj_w_listview *v, lj_surface *s, int ax, int ay, int show_sel)   /* glyph centred at the top of its tile, caption under it */
{
    const lj_font *f = lj_w_font_of(&v->base);
    int cols = icon_cols(v), i, vis = visible_rows(v);
    char buf[64];
    for (i = v->sb.pos * cols; i < v->n && i < (v->sb.pos + vis) * cols; i++) {
        int tx = ax + LV_BORDER + (i % cols) * v->tile_w, ty = ay + rows_top(v) + (i / cols - v->sb.pos) * v->tile_h;
        const lj_surface *g = lj_glyph_get(v->items[i]);
        size_t k;
        int tw, sel = show_sel && i == v->index, cy = ty + 2 + (g ? g->h : 32) + 3;   /* the caption right under the icon */
        if (g) lj_glyph_paint(s, tx + (v->tile_w - g->w) / 2, ty + 2, v->items[i], 1);
        lj_str_copy(buf, sizeof buf, v->items[i]);
        k = strlen(buf);
        while (k > 0 && lj_text_width(f, buf) + 4 > v->tile_w) buf[--k] = 0;   /* one line, clipped at the tile edge */
        tw = lj_text_width(f, buf);
        if (sel) lj_fill(s, tx + (v->tile_w - tw) / 2 - 2, cy - 1, tw + 4, f->height + 2, LJ_CLR_HIGHLIGHT);
        lj_w_text(s, &v->base, tx + (v->tile_w - tw) / 2, cy, buf, sel ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);
        if (sel && v->base.focused) lj_focus_rect(s, tx + (v->tile_w - tw) / 2 - 2, cy - 1, tw + 4, f->height + 2);
    }
}

void lj_w_listview_icon_view(lj_widget *w, int tile_w, int tile_h)
{
    lj_w_listview *v = (lj_w_listview *)w;
    if (!w || w->kind != LJ_K_LISTVIEW) return;
    v->tile_w = tile_w > 0 && tile_h > 0 ? tile_w : 0; v->tile_h = tile_h;
    v->sb.pos = 0;
    lj_widget_invalidate(w);
}

int lj_w_listview_row_at(const lj_widget *w, int y)
{
    const lj_w_listview *v = (const lj_w_listview *)w;
    int row;
    if (y < rows_top(v) || y >= w->h - LV_BORDER) return -1;
    row = v->sb.pos + (y - rows_top(v)) / v->row_h;
    return row < v->n ? row : -1;
}

/* the col-th '\t'-separated field of s into buf */
static char *subitem(const char *s, int col, char *buf, size_t cap)
{
    size_t i = 0;
    while (col > 0 && *s) { if (*s++ == '\t') col--; }
    if (col > 0) { buf[0] = 0; return buf; }
    while (*s && *s != '\t' && i + 1 < cap) buf[i++] = *s++;
    buf[i] = 0;
    return buf;
}

static void lv_paint(lj_widget *w, lj_surface *s)
{
    lj_w_listview *v = (lj_w_listview *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, i, c, vis, sb, area_w, show_sel;
    char buf[256];
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, w->color);
    FIT(v);
    sb = needs_sb(v);
    vis = visible_rows(v);
    area_w = w->w - 2 * LV_BORDER - (sb ? LJ_SB_W : 0);
    show_sel = v->index >= 0 && (w->focused || !v->hide_sel);
    if (v->tile_w) { icon_paint(v, s, ax, ay, show_sel); if (sb) lj_w_sbar_paint(s, &v->sb, ax, ay, w->enabled); return; }
    for (c = 0; c < v->ncols; c++) {                                 /* AutoSize = LVSCW_AUTOSIZE_USEHEADER: last column fills, others fit their header */
        if (!lj_desc_bool(v->cols[c].autosize, 0) && v->cols[c].width >= 0) continue;   /* Width -1/-2 (LVSCW_AUTOSIZE / _USEHEADER) sizes the same way */
        if (c == v->ncols - 1) { int used = 0; for (i = 0; i < c; i++) used += v->col_w[i]; v->col_w[c] = area_w - used > 0 ? area_w - used : 0; }
        else v->col_w[c] = lj_text_width(f, v->cols[c].caption ? v->cols[c].caption : "") + 12;
    }
    for (i = 0; i < vis && v->sb.pos + i < v->n; i++) {
        int item = v->sb.pos + i, ry = ay + rows_top(v) + i * v->row_h, x = ax + LV_BORDER, ty = ry + (v->row_h - f->height) / 2;
        int sel = show_sel && item == v->index;
        if (ry + v->row_h > ay + w->h - LV_BORDER) break;
        if (sel && v->row_select) lj_fill(s, x, ry, area_w, v->row_h, LJ_CLR_HIGHLIGHT);
        for (c = 0; c < v->ncols && x < ax + LV_BORDER + area_w; c++) {
            char *t = subitem(v->items[item], c, buf, sizeof buf);
            size_t k = strlen(t);
            int tw, tx = x + 2, cw = ax + LV_BORDER + area_w - x < v->col_w[c] ? ax + LV_BORDER + area_w - x : v->col_w[c];   /* the last column stops at the view's edge */
            while (k > 0 && lj_text_width(f, t) + 4 > cw) t[--k] = 0;   /* ME-111: clipped per glyph at the cell edge like the header (lj_w_header_paint), never dropped */
            tw = lj_text_width(f, t);
            if (v->cols[c].alignment == LJ_TA_RIGHT) tx = x + cw - 2 - tw;
            else if (v->cols[c].alignment == LJ_TA_CENTER) tx = x + (cw - tw) / 2;
            if (c == 0 && sel && !v->row_select) lj_fill(s, x + 2, ry, tw + 4 < cw ? tw + 4 : cw - 2, v->row_h, LJ_CLR_HIGHLIGHT);
            if (*t) lj_w_text(s, w, tx, ty, t, sel && (v->row_select || c == 0) ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);
            if (v->grid_lines && cw == v->col_w[c]) lj_vline(s, x + cw - 1, ry, v->row_h, LJ_CLR_BTNFACE);
            x += cw;
        }
        if (v->grid_lines) lj_hline(s, ax + LV_BORDER, ry + v->row_h - 1, area_w, LJ_CLR_BTNFACE);
        if (sel && w->focused) lj_focus_rect(s, ax + LV_BORDER, ry, area_w, v->row_h);
    }
    if (v->show_headers) lj_w_header_paint(s, w, ax + LV_BORDER, ay + LV_BORDER, area_w, v->header_h, v->cols, v->col_w, v->ncols, -1);
    if (sb) lj_w_sbar_paint(s, &v->sb, ax, ay, w->enabled);
}

static void select_row(lj_w_listview *v, int row)
{
    if (row < 0 || row >= v->n || row == v->index) return;
    v->index = row;
    FIT(v); lj_w_sbar_reveal(&v->sb, line_of(v, v->index), visible_rows(v));
    lj_widget_invalidate(&v->base);
    lj_w_emit(&v->base, LJ_WEV_SELECT_ITEM);
    lj_w_emit(&v->base, LJ_WEV_CHANGE);   /* W4 acceptance: TListView.OnChange fires on a selection change (ctState); Fixture Configuration's lvConfigChange never ran */
    lj_w_emit(&v->base, LJ_WEV_CLICK);
}

int lj_w_listview_index(const lj_widget *w) { return w ? ((const lj_w_listview *)w)->index : -1; }
void lj_w_listview_select(lj_widget *w, int row) { if (w) select_row((lj_w_listview *)w, row); }

static int lv_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_listview *v = (lj_w_listview *)w;
    int row, hx = m->x - LV_BORDER;
    if (!w->enabled) return 0;
    FIT(v);
    if (m->type == LJ_EV_MOUSE_WHEEL) { if (lj_w_sbar_set(&v->sb, v->sb.pos - 3 * m->wheel)) lj_widget_invalidate(w); return 1; }
    if (v->drag_col > 0) {
        if (m->type == LJ_EV_MOUSE_MOVE) { v->col_w[v->drag_col - 1] = hx - v->drag_x > 0 ? hx - v->drag_x : 0; lj_widget_invalidate(w); }
        if (m->type == LJ_EV_MOUSE_UP) v->drag_col = 0;
        return 1;
    }
    if (needs_sb(v) || v->sb.drag) {
        int r = lj_w_sbar_mouse(&v->sb, m);
        if (r == 1) lj_widget_invalidate(w);
        if (r >= 0) return 1;
    }
    if (v->show_headers && m->y >= LV_BORDER && m->y < rows_top(v)) {
        int col;
        if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1) {
            col = lj_w_header_divider_at(v->col_w, v->ncols, hx);
            if (col >= 0) { v->drag_col = col + 1; v->drag_x = hx - v->col_w[col]; return 1; }
            v->clicked_col = lj_w_header_section_at(v->col_w, v->ncols, hx);
        } else if (m->type == LJ_EV_MOUSE_UP && m->button == 1 && v->clicked_col >= 0) {
            if (lj_w_header_section_at(v->col_w, v->ncols, hx) == v->clicked_col) lj_w_emit(w, LJ_WEV_COLUMN_CLICK);
            v->clicked_col = -1;
        }
        return 1;
    }
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return m->type == LJ_EV_MOUSE_DOWN;
    lj_widget_set_focus(w);
    row = v->tile_w ? icon_at(v, m->x, m->y) : lj_w_listview_row_at(w, m->y);
    if (row >= 0 && row == v->index) lj_w_emit(w, LJ_WEV_CLICK);   /* VCL OnClick fires on every press, not only on a new row: a dmAutomatic source drags its selected row */
    else if (row >= 0) select_row(v, row);
    if (row >= 0 && m->clicks == 2) lj_w_emit(w, LJ_WEV_DBLCLICK);
    return 1;
}

static int lv_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_listview *v = (lj_w_listview *)w;
    int step = v->tile_w ? icon_cols(v) : 1, vis = visible_rows(v) * step, next;   /* ME-160: Up / Down move a tile line in the vsIcon view */
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled || v->n == 0) return 0;
    switch (k->key) {
    case LJ_VK_UP:    next = v->index - step > 0 ? v->index - step : 0; break;
    case LJ_VK_DOWN:  next = v->index + step < v->n ? v->index + step : v->n - 1; break;
    case LJ_VK_LEFT:  if (!v->tile_w) return 0; next = v->index > 0 ? v->index - 1 : 0; break;
    case LJ_VK_RIGHT: if (!v->tile_w) return 0; next = v->index + 1 < v->n ? v->index + 1 : v->n - 1; break;
    case LJ_VK_PRIOR: next = v->index - vis > 0 ? v->index - vis : 0; break;
    case LJ_VK_NEXT:  next = v->index + vis < v->n ? v->index + vis : v->n - 1; break;
    case LJ_VK_HOME:  next = 0; break;
    case LJ_VK_END:   next = v->n - 1; break;
    default: return 0;
    }
    select_row(v, next);
    return 1;
}

static const lj_widget_vt lv_vt = { lv_paint, lv_mouse, lv_key, NULL, NULL };

lj_widget *lj_w_listview_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_listview *v = lj_w_alloc(sizeof *v, parent, d, &lv_vt, 1);
    int i;
    v->index = -1; v->row_h = 14; v->header_h = 17;   /* rig item: report row pitch for MS Sans Serif 13, measured later */
    v->clicked_col = -1;
    v->show_headers = 1; v->hide_sel = 1;
    v->base.color = LJ_CLR_BTNHIGHLIGHT;
    if (d) {
        v->base.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        v->items = d->items; v->n = d->items ? d->nitems : 0;
        v->cols = d->columns; v->ncols = d->columns ? (d->ncolumns < LJ_LV_COLS ? d->ncolumns : LJ_LV_COLS) : 0;
        for (i = 0; i < v->ncols; i++) v->col_w[i] = d->columns[i].width > 0 ? d->columns[i].width : d->columns[i].width < 0 ? 0 : 50;   /* TListColumn.Width default; < 0 = autosize at paint */
        v->show_headers = lj_desc_bool(d->show_column_headers, 1) != 0;
        v->row_select = lj_desc_bool(d->row_select, 0) != 0;
        v->grid_lines = lj_desc_bool(d->grid_lines, 0) != 0;
        v->hide_sel = lj_desc_bool(d->hide_selection, 1) != 0;
    }
    return &v->base;
}

void lj_w_listview_set_items(lj_widget *w, const char *const *items, int n)
{
    lj_w_listview *v = (lj_w_listview *)w;
    if (items != v->items || (items ? n : 0) != v->n) v->sb.pos = 0;   /* a refresh of the same rows keeps the scroll (HTP Groups refills every 100 ms; rows 13-24 were unreachable) */
    v->items = items; v->n = items ? n : 0;
    v->index = -1;
    lj_widget_invalidate(w);
}
