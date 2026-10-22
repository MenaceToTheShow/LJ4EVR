/* lj_w_stringgrid.c - TStringGrid (design 7.2 "minimal grid variant"): sunken box,
 * fixed cells raised (face, white TL / #808080 BR), data cells clWindow with
 * #C0C0C0 lines, navy selected cell (whole row with goRowSelect), vertical
 * scrollbar over the data rows, arrow/page/home/end keys. Cell strings are
 * rows*cols caller-owned pointers in the widget's own allocation.
 * Also the shared paint/mouse/key for TDrawGrid (cells == NULL, OnDrawCell per cell). */
#include "ui/widgets/lj_widgets_lists.h"

#include <string.h>

#define GR_BORDER 2
#define GR_LINE   1

static int data_rows_h(const lj_w_grid *g) { return g->base.h - GR_BORDER * 2 - g->fixed_rows * (g->def_rh + GR_LINE); }
static int visible_data_rows(const lj_w_grid *g) { int v = data_rows_h(g) / (g->def_rh + GR_LINE); return v < 1 ? 1 : v; }
static int needs_sb(const lj_w_grid *g) { return (g->rows - g->fixed_rows) * (g->def_rh + GR_LINE) > data_rows_h(g); }

static void fit(lj_w_grid *g)   /* top_row is the grid's own scroll state; the bar mirrors it */
{
    g->sb.pos = g->top_row;
    lj_w_sbar_fit(&g->sb, g->base.w, GR_BORDER, GR_BORDER, g->base.h - 2 * GR_BORDER, g->rows - g->fixed_rows, visible_data_rows(g));
    g->top_row = g->sb.pos;
}

static int col_x(const lj_w_grid *g, int col)   /* left of col from the grid's inner edge */
{
    int i, x = 0;
    if (!g->col_w) return col * (g->def_cw + GR_LINE);
    for (i = 0; i < col; i++) x += g->col_w[i] + GR_LINE;
    return x;
}

int lj_w_grid_cell_rect(const lj_widget *w, int col, int row, lj_rect *out)
{
    const lj_w_grid *g = (const lj_w_grid *)w;
    int slot;
    if (col < 0 || col >= g->cols || row < 0 || row >= g->rows) return -1;
    slot = row < g->fixed_rows ? row : row - g->top_row;
    if (row >= g->fixed_rows && (row - g->fixed_rows < g->top_row)) return -1;
    out->x = GR_BORDER + col_x(g, col);
    out->y = GR_BORDER + slot * (g->def_rh + GR_LINE);
    out->w = g->col_w ? g->col_w[col] : g->def_cw; out->h = g->def_rh;
    return out->y + out->h <= w->h - GR_BORDER ? 0 : -1;
}

int lj_w_grid_cell_at(const lj_widget *w, int x, int y, int *col, int *row)
{
    const lj_w_grid *g = (const lj_w_grid *)w;
    int c, r;
    if (x < GR_BORDER || y < GR_BORDER || x >= w->w - GR_BORDER || y >= w->h - GR_BORDER) return -1;
    for (c = 0; c < g->cols && x - GR_BORDER >= col_x(g, c + 1); c++) { }
    r = (y - GR_BORDER) / (g->def_rh + GR_LINE);
    if (r >= g->fixed_rows) r += g->top_row;
    if (c >= g->cols || r >= g->rows) return -1;
    *col = c; *row = r;
    return 0;
}

static void grid_paint(lj_widget *w, lj_surface *s)
{
    lj_w_grid *g = (lj_w_grid *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, c, r, sb;
    lj_rect cr;
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, w->color);
    fit(g);
    sb = needs_sb(g);
    for (r = 0; r < g->rows; r++) {
        if (r >= g->fixed_rows && r - g->fixed_rows < g->top_row) continue;
        for (c = 0; c < g->cols; c++) {
            int fixed = r < g->fixed_rows || c < g->fixed_cols, x, y;
            int sel = !fixed && w->enabled && (r == g->row) && (g->row_select || c == g->col);
            const char *t = g->cells ? g->cells[r * g->cols + c] : NULL;
            if (lj_w_grid_cell_rect(w, c, r, &cr) != 0) continue;
            x = ax + cr.x; y = ay + cr.y;
            if (sb && x + cr.w > ax + g->sb.x) continue;
            if (fixed) {
                lj_fill(s, x, y, cr.w, cr.h, LJ_CLR_BTNFACE);
                if (g->vlines || g->hlines) lj_bevel1(s, x, y, cr.w, cr.h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
                lj_fill(s, x + cr.w, y, GR_LINE, cr.h + GR_LINE, LJ_CLR_BTNFACE);
                lj_fill(s, x, y + cr.h, cr.w, GR_LINE, LJ_CLR_BTNFACE);
            } else {
                lj_fill(s, x, y, cr.w, cr.h, sel ? LJ_CLR_HIGHLIGHT : w->color);
                if (g->vlines) lj_vline(s, x + cr.w, y, cr.h + GR_LINE, LJ_CLR_BTNFACE);
                if (g->hlines) lj_hline(s, x, y + cr.h, cr.w + GR_LINE, LJ_CLR_BTNFACE);
            }
            if (t && *t) lj_w_text(s, w, x + 2, y + (cr.h - f->height) / 2, t, sel ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);
            if (!g->cells && w->on_event) {
                g->draw_col = c; g->draw_row = r; g->draw_rect.x = x; g->draw_rect.y = y; g->draw_rect.w = cr.w; g->draw_rect.h = cr.h; g->draw_surf = s;
                lj_w_emit(w, LJ_WEV_DRAW_CELL);
            }
            if (sel && w->focused && (c == g->col || !g->row_select)) lj_focus_rect(s, x, y, cr.w, cr.h);
        }
    }
    if (sb) lj_w_sbar_paint(s, &g->sb, ax, ay, w->enabled);
}


static void select_cell(lj_w_grid *g, int col, int row)
{
    if (col < g->fixed_cols) col = g->fixed_cols;
    if (row < g->fixed_rows) row = g->fixed_rows;
    if (col >= g->cols) col = g->cols - 1;
    if (row >= g->rows) row = g->rows - 1;
    if (col == g->col && row == g->row) return;
    g->col = col; g->row = row;
    g->sb.pos = g->top_row; lj_w_sbar_reveal(&g->sb, g->row - g->fixed_rows, visible_data_rows(g)); g->top_row = g->sb.pos;
    lj_widget_invalidate(&g->base);
    lj_w_emit(&g->base, LJ_WEV_SELECT_CELL);
    lj_w_emit(&g->base, LJ_WEV_CLICK);
}

static int grid_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_grid *g = (lj_w_grid *)w;
    int col, row;
    if (!w->enabled) return 0;
    fit(g);
    if (m->type == LJ_EV_MOUSE_WHEEL) { if (lj_w_sbar_set(&g->sb, g->sb.pos - 3 * m->wheel)) { g->top_row = g->sb.pos; lj_widget_invalidate(w); } return 1; }
    if (needs_sb(g) || g->sb.drag) {
        int r = lj_w_sbar_mouse(&g->sb, m);
        if (r == 1) { g->top_row = g->sb.pos; lj_widget_invalidate(w); }
        if (r >= 0) return 1;
    }
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return m->type == LJ_EV_MOUSE_DOWN;
    lj_widget_set_focus(w);
    if (lj_w_grid_cell_at(w, m->x, m->y, &col, &row) == 0 && col >= g->fixed_cols && row >= g->fixed_rows) {
        select_cell(g, col, row);
        if (m->clicks == 2) lj_w_emit(w, LJ_WEV_DBLCLICK);
    }
    return 1;
}

static int grid_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_grid *g = (lj_w_grid *)w;
    int vis = visible_data_rows(g);
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled) return 0;
    switch (k->key) {
    case LJ_VK_UP:    select_cell(g, g->col, g->row - 1); return 1;
    case LJ_VK_DOWN:  select_cell(g, g->col, g->row + 1); return 1;
    case LJ_VK_LEFT:  select_cell(g, g->col - 1, g->row); return 1;
    case LJ_VK_RIGHT: select_cell(g, g->col + 1, g->row); return 1;
    case LJ_VK_PRIOR: select_cell(g, g->col, g->row - vis); return 1;
    case LJ_VK_NEXT:  select_cell(g, g->col, g->row + vis); return 1;
    case LJ_VK_HOME:  select_cell(g, g->fixed_cols, g->row); return 1;
    case LJ_VK_END:   select_cell(g, g->cols - 1, g->row); return 1;
    default: return 0;
    }
}

static const lj_widget_vt grid_vt = { grid_paint, grid_mouse, grid_key, NULL };

lj_widget *lj_w_grid_create(lj_widget *parent, const lj_ctl_desc *d, int with_cells)
{
    int rows = 5, cols = 5;
    lj_w_grid *g;
    if (d) { if (d->row_count > 0) rows = d->row_count; if (d->col_count > 0) cols = d->col_count; }
    g = lj_w_alloc(sizeof *g + (with_cells ? (size_t)rows * (size_t)cols * sizeof(const char *) : 0), parent, d, &grid_vt, 1);
    g->rows = rows; g->cols = cols;
    g->fixed_rows = 1; g->fixed_cols = 1; g->def_rh = 24; g->def_cw = 64;
    g->vlines = 1; g->hlines = 1;
    g->base.color = LJ_CLR_BTNHIGHLIGHT;
    if (with_cells) g->cells = (const char **)(g + 1);
    if (d) {
        g->base.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        g->fixed_rows = d->fixed_rows < 0 ? 0 : (d->fixed_rows < rows ? d->fixed_rows : rows - 1);   /* the generator always writes these (lj_form.h defaults rule) */
        g->fixed_cols = d->fixed_cols < 0 ? 0 : (d->fixed_cols < cols ? d->fixed_cols : cols - 1);
        if (d->default_row_height > 0) g->def_rh = d->default_row_height;
        if (d->default_col_width > 0) g->def_cw = d->default_col_width;
        if (d->options) {
            g->row_select = strstr(d->options, "goRowSelect") != NULL;
            g->vlines = strstr(d->options, "goVertLine") != NULL || strstr(d->options, "goFixedVertLine") != NULL;
            g->hlines = strstr(d->options, "goHorzLine") != NULL || strstr(d->options, "goFixedHorzLine") != NULL;
        }
    }
    g->col = g->fixed_cols; g->row = g->fixed_rows;
    return &g->base;
}

lj_widget *lj_w_stringgrid_create(lj_widget *parent, const lj_ctl_desc *d) { return lj_w_grid_create(parent, d, 1); }

int lj_w_grid_set_cell(lj_widget *w, int col, int row, const char *text)
{
    lj_w_grid *g = (lj_w_grid *)w;
    if (!g->cells || col < 0 || col >= g->cols || row < 0 || row >= g->rows) return -1;
    g->cells[row * g->cols + col] = text;
    lj_widget_invalidate(w);
    return 0;
}
