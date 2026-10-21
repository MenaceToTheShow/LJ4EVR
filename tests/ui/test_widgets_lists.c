/* test_widgets_lists.c - the runnable check for src/ui/widgets/ lists group: every
 * class built from a hand-written lj_ctl_desc, painted into a surface, key pixels
 * asserted (bevel colours, face, text present by non-face count), then driven
 * with mouse/key events and the state change asserted (design 7.3, 7.9). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/widgets/lj_widgets_lists.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int ev_count[LJ_WEV_LISTS_COUNT];
static void on_ev(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev >= 0 && ev < LJ_WEV_LISTS_COUNT) ev_count[ev]++; }
static void bind(lj_widget *w) { w->on_event = on_ev; memset(ev_count, 0, sizeof ev_count); }

static uint32_t px(const lj_surface *s, int x, int y) { return (x < 0 || y < 0 || x >= s->w || y >= s->h) ? 0xDEADBEEFu : s->px[y * s->w + x]; }
static int count_not(const lj_surface *s, int x, int y, int w, int h, uint32_t c) { int i, j, n = 0; for (j = 0; j < h; j++) for (i = 0; i < w; i++) if (px(s, x + i, y + j) != c) n++; return n; }
static int count_eq(const lj_surface *s, int x, int y, int w, int h, uint32_t c) { return w * h - count_not(s, x, y, w, h, c); }

static void paint_tree(lj_widget *w, lj_surface *s)
{
    lj_widget *c;
    if (!w->visible) return;
    if (w->vt && w->vt->paint) w->vt->paint(w, s);
    for (c = w->first; c; c = c->next) paint_tree(c, s);
}
static void repaint(lj_widget *root, lj_surface *s) { lj_fill(s, 0, 0, s->w, s->h, LJ_CLR_BTNFACE); lj_widget_layout(root); paint_tree(root, s); }

static void mouse(lj_widget *w, int type, int x, int y, int clicks)
{
    lj_mouse_ev m; memset(&m, 0, sizeof m);
    m.type = type; m.x = x; m.y = y; m.button = 1; m.clicks = clicks;
    if (w->vt->mouse) w->vt->mouse(w, &m);
}
static void click(lj_widget *w, int x, int y) { mouse(w, LJ_EV_MOUSE_DOWN, x, y, 1); mouse(w, LJ_EV_MOUSE_UP, x, y, 1); }
static void wheel(lj_widget *w, int notches) { lj_mouse_ev m; memset(&m, 0, sizeof m); m.type = LJ_EV_MOUSE_WHEEL; m.wheel = notches; w->vt->mouse(w, &m); }
static int key(lj_widget *w, int vk)
{
    lj_key_ev k; memset(&k, 0, sizeof k);
    k.type = LJ_EV_KEY_DOWN; k.key = vk;
    return w->vt->key ? w->vt->key(w, &k) : 0;
}

static void free_children(lj_widget *root) { lj_widget *c, *n; for (c = root->first; c; c = n) { n = c->next; free_children(c); free(c); } root->first = NULL; }

static const char *const five[] = { "Alpha", "Bravo", "Charlie", "Delta", "Echo" };
static const char *const rows[] = { "1\tAlpha\t10", "2\tBravo\t20", "3\tCharlie\t30", "4\tDelta\t40" };
static const lj_column_desc cols[] = { { "No", 40, LJ_TA_UNSET }, { "Name", 80, LJ_TA_UNSET }, { "Val", 50, LJ_TA_RIGHT } };
static const char *const tree[] = { "A", "\tA1", "\tA2", "\t\tA2x", "B" };
static const char *const tabs3[] = { "One", "Two", "Three" };
static const char *lines20[20];

static lj_widget *hosted; static int hosted_x, hosted_y, closed;
static int hook_open(lj_widget *c, lj_widget *l, int ax, int ay, void *ud) { (void)c; (void)ud; hosted = l; hosted_x = ax; hosted_y = ay; return 0; }
static void hook_close(lj_widget *c, lj_widget *l, void *ud) { (void)c; (void)ud; closed += l == hosted; }

int lj_test_widgets_lists(int argc, char **argv);
int lj_test_widgets_lists(int argc, char **argv)
{
    lj_widget root, *w, *p0, *p1;
    lj_surface s;
    lj_ctl_desc d;
    int i, before;
    (void)argc; (void)argv;

    memset(&root, 0, sizeof root);
    root.w = 400; root.h = 300; root.visible = 1; root.enabled = 1; root.color = LJ_CLR_BTNFACE; root.font = &lj_font_sans13; root.name = "root";
    CHECK(lj_surface_init(&s, 400, 300) == 0);

    /* registration */
    {
        lj_widget_ctor table[LJ_K_COUNT]; int n = 0;
        memset(table, 0, sizeof table);
        CHECK(lj_widgets_register_lists(table, LJ_K_COUNT) == 0);
        CHECK(lj_widgets_register_lists(table, 3) == -1);
        for (i = 0; i < LJ_K_COUNT; i++) n += table[i] != NULL;
        CHECK(n == 17 && table[LJ_K_LISTVIEW] == lj_w_listview_create && table[LJ_K_LABEL] == NULL);
        CHECK(strcmp(lj_wev_lists_names[LJ_WEV_SCROLL - LJ_WEV_COUNT], "OnScroll") == 0);
    }

    /* ---- TListBox: sunken edge, text, click selects navy, keys ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_LISTBOX; d.name = "lb"; d.left = 10; d.top = 10; d.width = 120; d.height = 80; d.items = five; d.nitems = 5; d.item_height = 13; d.item_index = -1;   /* -1 = none, as the generator writes it */
    w = lj_w_listbox_create(&root, &d); bind(w);
    CHECK(w && w->parent == &root && w->kind == LJ_K_LISTBOX && w->tabstop && lj_w_font_of(w) == &lj_font_sans13);
    repaint(&root, &s);
    CHECK(px(&s, 10, 10) == LJ_CLR_BTNSHADOW && px(&s, 11, 11) == LJ_CLR_3DDKSHADOW);          /* outer #808080, inner black TL */
    CHECK(px(&s, 129, 89) == LJ_CLR_BTNHIGHLIGHT && px(&s, 128, 88) == LJ_CLR_BTNFACE);         /* outer white, inner #C0C0C0 BR */
    CHECK(px(&s, 125, 15) == LJ_CLR_BTNHIGHLIGHT);                                              /* clWindow interior */
    CHECK(count_not(&s, 14, 12, 40, 13, LJ_CLR_BTNHIGHLIGHT) > 10);                              /* "Alpha" present */
    CHECK(count_not(&s, 14, 12 + 65, 100, 8, LJ_CLR_BTNHIGHLIGHT) == 0);                         /* below the 5 rows: empty */
    click(w, 20, 2 + 2 * 13 + 3);
    CHECK(((lj_w_list *)w)->index == 2 && w->focused && ev_count[LJ_WEV_CLICK] == 1);
    repaint(&root, &s);
    CHECK(px(&s, 100, 10 + 2 + 26 + 6) == LJ_CLR_HIGHLIGHT);                                    /* row 2 navy */
    CHECK(count_eq(&s, 14, 10 + 2 + 26, 40, 13, LJ_CLR_BTNHIGHLIGHT) > 5);                       /* white text on it */
    CHECK(key(w, LJ_VK_DOWN) && ((lj_w_list *)w)->index == 3);
    CHECK(key(w, LJ_VK_END) && ((lj_w_list *)w)->index == 4 && key(w, LJ_VK_HOME) && ((lj_w_list *)w)->index == 0);
    CHECK(ev_count[LJ_WEV_CLICK] == 4);
    mouse(w, LJ_EV_MOUSE_DOWN, 20, 5, 2);
    CHECK(ev_count[LJ_WEV_DBLCLICK] == 1);
    w->enabled = 0; repaint(&root, &s);
    CHECK(count_eq(&s, 14, 12, 40, 14, LJ_CLR_BTNSHADOW) > 5);                                  /* disabled: #808080 text */
    CHECK(!key(w, LJ_VK_DOWN));
    w->enabled = 1;
    /* overflow: scrollbar appears, wheel scrolls */
    lj_w_list_set_items(w, five, 5); w->h = 40; repaint(&root, &s);
    CHECK(px(&s, 10 + 120 - 2 - 16, 12) == LJ_CLR_BTNHIGHLIGHT && px(&s, 10 + 120 - 3, 12 + 15) == LJ_CLR_3DDKSHADOW);   /* up-arrow button raised */
    wheel(w, -1);
    CHECK(((lj_w_list *)w)->sb.pos == 3);
    click(w, 110, 2 + 3);                                                                       /* up arrow, widget-relative */
    CHECK(((lj_w_list *)w)->sb.pos == 2);
    free_children(&root);

    /* ---- TCheckListBox: box toggles on click and Space ---- */
    d.kind = LJ_K_CHECKLISTBOX; d.height = 80;
    w = lj_w_checklistbox_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(px(&s, 10 + 3, 10 + 2 + 1) == LJ_CLR_BTNSHADOW && px(&s, 10 + 3 + 6, 10 + 2 + 6) == LJ_CLR_BTNHIGHLIGHT);   /* 13x13 sunken square, white inside */
    before = count_not(&s, 10 + 3, 10 + 2 + 13, 13, 13, LJ_CLR_BTNHIGHLIGHT);
    click(w, 6, 2 + 13 + 6);
    CHECK(lj_w_list_checked(w, 1) == 1 && ev_count[LJ_WEV_CLICK_CHECK] == 1 && ((lj_w_list *)w)->index == 1);
    repaint(&root, &s);
    CHECK(count_not(&s, 10 + 3, 10 + 2 + 13, 13, 13, LJ_CLR_BTNHIGHLIGHT) > before);                /* the check glyph */
    CHECK(key(w, LJ_VK_SPACE) && lj_w_list_checked(w, 1) == 0 && ev_count[LJ_WEV_CLICK_CHECK] == 2);
    click(w, 60, 2 + 6);
    CHECK(lj_w_list_checked(w, 0) == 0 && ((lj_w_list *)w)->index == 0);                          /* text click selects, no toggle */
    free_children(&root);

    /* ---- TComboBox: drop button, in-client drop list, pick, hook ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_COMBOBOX; d.name = "cb"; d.left = 20; d.top = 20; d.width = 100; d.height = 21; d.items = tabs3; d.nitems = 3; d.style = "csDropDownList"; d.item_index = 0; d.item_height = 13; d.drop_down_count = 8;
    w = lj_w_combobox_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(px(&s, 20, 20) == LJ_CLR_BTNSHADOW && px(&s, 20 + 100 - 18, 22) == LJ_CLR_BTNHIGHLIGHT && px(&s, 20 + 100 - 3, 20 + 21 - 3) == LJ_CLR_3DDKSHADOW);   /* sunken box, raised button */
    CHECK(count_not(&s, 24, 24, 30, 13, LJ_CLR_BTNHIGHLIGHT) > 5);                               /* "One" shown */
    click(w, 90, 10);
    CHECK(((lj_w_combobox *)w)->drop != NULL && root.first->next == ((lj_w_combobox *)w)->drop);   /* fallback: last child of root */
    CHECK(((lj_w_combobox *)w)->drop->y == 41 && ((lj_w_combobox *)w)->drop->h == 3 * 13 + 4);
    repaint(&root, &s);
    CHECK(px(&s, 20, 41) == LJ_CLR_3DDKSHADOW && px(&s, 60, 41 + 2 + 13 + 6) == LJ_CLR_BTNHIGHLIGHT);   /* 1-px black frame, white rows */
    {
        lj_widget *dl = ((lj_w_combobox *)w)->drop;
        mouse(dl, LJ_EV_MOUSE_MOVE, 30, 2 + 13 + 6, 0);
        CHECK(((lj_w_list *)dl)->index == 1);
        mouse(dl, LJ_EV_MOUSE_UP, 30, 2 + 2 * 13 + 6, 1);
    }
    CHECK(((lj_w_combobox *)w)->drop == NULL && ((lj_w_combobox *)w)->index == 2 && root.first->next == NULL);
    CHECK(ev_count[LJ_WEV_CHANGE] == 1 && ev_count[LJ_WEV_CLICK] == 1);
    CHECK(key(w, LJ_VK_UP) && ((lj_w_combobox *)w)->index == 1 && ev_count[LJ_WEV_CHANGE] == 2);
    CHECK(key(w, LJ_VK_F4) && ((lj_w_combobox *)w)->drop && key(w, LJ_VK_DOWN) && key(w, LJ_VK_RETURN));
    CHECK(((lj_w_combobox *)w)->drop == NULL && ((lj_w_combobox *)w)->index == 2);
    CHECK(key(w, LJ_VK_F4) && key(w, LJ_VK_ESCAPE) && ((lj_w_combobox *)w)->drop == NULL && ((lj_w_combobox *)w)->index == 2);
    {
        lj_w_popup_hook hk; memset(&hk, 0, sizeof hk);
        hk.open = hook_open; hk.close = hook_close;
        lj_w_combobox_set_popup_hook(w, &hk);
        CHECK(lj_w_combobox_drop(w) == 0 && hosted == ((lj_w_combobox *)w)->drop && hosted->parent == NULL && root.first->next == NULL);
        CHECK(hosted_x == 20 && hosted_y == 41);
        ((lj_w_list *)hosted)->index = 0;
        lj_w_combobox_close(w, 1);
        CHECK(closed == 1 && ((lj_w_combobox *)w)->index == 0 && ((lj_w_combobox *)w)->drop == NULL);
        lj_w_combobox_set_popup_hook(w, NULL);
    }
    free_children(&root);

    /* ---- TListView: header, rows, selection, column click, divider drag ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_LISTVIEW; d.name = "lv"; d.left = 10; d.top = 10; d.width = 200; d.height = 100; d.items = rows; d.nitems = 4; d.columns = cols; d.ncolumns = 3; d.row_select = LJ_YES;
    w = lj_w_listview_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(px(&s, 12, 12) == LJ_CLR_BTNHIGHLIGHT && px(&s, 12 + 39, 12 + 16) == LJ_CLR_3DDKSHADOW);   /* header section raised: white TL, black BR */
    CHECK(count_not(&s, 16, 14, 20, 13, LJ_CLR_BTNFACE) > 5);                                   /* "No" caption */
    CHECK(px(&s, 100, 12 + 17 + 5) == LJ_CLR_BTNHIGHLIGHT);                                     /* row 0 white, no selection */
    CHECK(count_not(&s, 12 + 42, 12 + 17, 40, 14, LJ_CLR_BTNHIGHLIGHT) > 5);                     /* "Alpha" in column 1 */
    click(w, 50, 2 + 17 + 14 + 5);
    CHECK(((lj_w_listview *)w)->index == 1 && w->focused && ev_count[LJ_WEV_SELECT_ITEM] == 1);
    repaint(&root, &s);
    CHECK(px(&s, 100, 12 + 17 + 14 + 5) == LJ_CLR_HIGHLIGHT && px(&s, 12 + 120 + 5, 12 + 17 + 14 + 5) == LJ_CLR_HIGHLIGHT);   /* whole row navy (RowSelect), probes clear of the text */
    w->focused = 0; repaint(&root, &s);
    CHECK(px(&s, 100, 12 + 17 + 14 + 5) == LJ_CLR_BTNHIGHLIGHT);                                /* HideSelection */
    w->focused = 1;
    CHECK(key(w, LJ_VK_DOWN) && ((lj_w_listview *)w)->index == 2 && key(w, LJ_VK_END) && ((lj_w_listview *)w)->index == 3);
    click(w, 2 + 60, 2 + 8);
    CHECK(ev_count[LJ_WEV_COLUMN_CLICK] == 1 && ((lj_w_listview *)w)->clicked_col == -1);
    mouse(w, LJ_EV_MOUSE_DOWN, 2 + 40, 2 + 8, 1); mouse(w, LJ_EV_MOUSE_MOVE, 2 + 60, 2 + 8, 0); mouse(w, LJ_EV_MOUSE_UP, 2 + 60, 2 + 8, 1);
    CHECK(((lj_w_listview *)w)->col_w[0] == 60 && ev_count[LJ_WEV_COLUMN_CLICK] == 1);
    CHECK(lj_w_listview_row_at(w, 2 + 17 + 3 * 14 + 1) == 3 && lj_w_listview_row_at(w, 2 + 17 + 4 * 14 + 1) == -1);
    free_children(&root);

    /* ---- THeaderControl ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_HEADERCONTROL; d.name = "hc"; d.left = 0; d.top = 0; d.width = 300; d.height = 17; d.columns = cols; d.ncolumns = 2;
    w = lj_w_headercontrol_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(px(&s, 0, 0) == LJ_CLR_BTNHIGHLIGHT && px(&s, 39, 16) == LJ_CLR_3DDKSHADOW && px(&s, 38, 15) == LJ_CLR_BTNSHADOW && px(&s, 200, 8) == LJ_CLR_BTNFACE);
    click(w, 70, 8);
    CHECK(ev_count[LJ_WEV_SECTION_CLICK] == 1 && ((lj_w_headercontrol *)w)->clicked == 1);
    mouse(w, LJ_EV_MOUSE_DOWN, 41, 8, 1); mouse(w, LJ_EV_MOUSE_MOVE, 70, 8, 0); mouse(w, LJ_EV_MOUSE_UP, 70, 8, 1);
    CHECK(((lj_w_headercontrol *)w)->widths[0] == 69 && ev_count[LJ_WEV_SECTION_RESIZE] == 1 && ev_count[LJ_WEV_SECTION_CLICK] == 1);
    free_children(&root);

    /* ---- TTreeView: +/- box, lines, expand/collapse, keys ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_TREEVIEW; d.name = "tv"; d.left = 10; d.top = 10; d.width = 150; d.height = 120; d.items = tree; d.nitems = 5;
    w = lj_w_treeview_create(&root, &d); bind(w);
    CHECK(lj_w_treeview_visible_count(w) == 2 && lj_w_treeview_visible_item(w, 1) == 4 && lj_w_treeview_level(w, 3) == 2);
    repaint(&root, &s);
    {
        int nx = 10 + 2 + 19 / 2, cy = 10 + 2 + 8;                                              /* node_x for level 0 with ShowRoot, row 0 centre */
        CHECK(px(&s, nx - 4, cy - 4) == LJ_CLR_BTNSHADOW && px(&s, nx, cy) == LJ_CLR_3DDKSHADOW && px(&s, nx - 1, cy - 1) == LJ_CLR_BTNHIGHLIGHT);   /* +box: frame, cross, white fill */
        CHECK(px(&s, nx, cy - 2) == LJ_CLR_3DDKSHADOW);                                          /* the '+' vertical bar: collapsed */
        CHECK(count_eq(&s, nx + 5, cy, 6, 1, LJ_CLR_BTNSHADOW) >= 2);                            /* dotted line to the text */
        click(w, nx - 10, cy - 10);                                                              /* widget-relative */
        CHECK(lj_w_treeview_expanded(w, 0) && lj_w_treeview_visible_count(w) == 4);
        repaint(&root, &s);
        CHECK(px(&s, nx, cy - 2) == LJ_CLR_BTNHIGHLIGHT);                                        /* '-' now */
        click(w, 2 + 19 + 9 + 9 + 2 + 4, cy - 10 + 16);                                           /* A1 text: text_x(level 1) = 41 */
        CHECK(((lj_w_treeview *)w)->index == 1 && ev_count[LJ_WEV_CHANGE] == 1);
        CHECK(key(w, LJ_VK_DOWN) && ((lj_w_treeview *)w)->index == 2);
        CHECK(key(w, LJ_VK_RIGHT) && lj_w_treeview_expanded(w, 2) && key(w, LJ_VK_RIGHT) && ((lj_w_treeview *)w)->index == 3);
        CHECK(key(w, LJ_VK_LEFT) && ((lj_w_treeview *)w)->index == 2 && key(w, LJ_VK_LEFT) && !lj_w_treeview_expanded(w, 2));
        CHECK(key(w, LJ_VK_END) && ((lj_w_treeview *)w)->index == 4 && key(w, LJ_VK_HOME) && ((lj_w_treeview *)w)->index == 0);
        CHECK(key(w, LJ_VK_SUBTRACT) && !lj_w_treeview_expanded(w, 0) && lj_w_treeview_visible_count(w) == 2);
    }
    free_children(&root);

    /* ---- TStringGrid / TDrawGrid ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_STRINGGRID; d.name = "sg"; d.left = 0; d.top = 0; d.width = 300; d.height = 120; d.row_count = 3; d.col_count = 3; d.fixed_rows = 1; d.fixed_cols = 1; d.default_row_height = 24; d.default_col_width = 64; d.options = "[goFixedVertLine, goFixedHorzLine, goVertLine, goHorzLine]";
    w = lj_w_stringgrid_create(&root, &d); bind(w);
    CHECK(lj_w_grid_set_cell(w, 1, 1, "x") == 0 && lj_w_grid_set_cell(w, 3, 1, "x") == -1);
    repaint(&root, &s);
    CHECK(px(&s, 2, 2) == LJ_CLR_BTNHIGHLIGHT && px(&s, 2 + 63, 2 + 23) == LJ_CLR_BTNSHADOW && px(&s, 12, 12) == LJ_CLR_BTNFACE);   /* fixed cell raised */
    CHECK(px(&s, 2 + 65 + 10, 2 + 25 + 10) == LJ_CLR_HIGHLIGHT);                                /* the selected cell (1,1) navy */
    CHECK(px(&s, 2 + 2 * 65 + 10, 2 + 2 * 25 + 10) == LJ_CLR_BTNHIGHLIGHT);                     /* data cell white */
    CHECK(px(&s, 2 + 65 + 64, 2 + 2 * 25 + 10) == LJ_CLR_BTNFACE);                               /* #C0C0C0 grid line */
    CHECK(count_eq(&s, 2 + 65, 2 + 25, 64, 24, LJ_CLR_BTNHIGHLIGHT) > 3);                        /* "x" white on navy */
    click(w, 2 + 2 * 65 + 5, 2 + 25 + 5);
    CHECK(((lj_w_grid *)w)->col == 2 && ((lj_w_grid *)w)->row == 1 && ev_count[LJ_WEV_SELECT_CELL] == 1);
    CHECK(key(w, LJ_VK_DOWN) && ((lj_w_grid *)w)->row == 2 && key(w, LJ_VK_LEFT) && ((lj_w_grid *)w)->col == 1);
    CHECK(key(w, LJ_VK_UP) && key(w, LJ_VK_UP) && ((lj_w_grid *)w)->row == 1);                   /* never into the fixed row */
    click(w, 10, 10);
    CHECK(((lj_w_grid *)w)->row == 1 && ((lj_w_grid *)w)->col == 1);                            /* fixed cell click does not select */
    free_children(&root);
    d.kind = LJ_K_DRAWGRID;
    w = lj_w_drawgrid_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(ev_count[LJ_WEV_DRAW_CELL] == 9 && ((lj_w_grid *)w)->cells == NULL && ((lj_w_grid *)w)->draw_rect.w == 64);
    free_children(&root);

    /* ---- TPageControl + TTabSheet ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_PAGECONTROL; d.name = "pc"; d.left = 10; d.top = 10; d.width = 200; d.height = 150;
    w = lj_w_pagecontrol_create(&root, &d); bind(w);
    {
        lj_ctl_desc t1, t2;
        memset(&t1, 0, sizeof t1); t1.kind = LJ_K_TABSHEET; t1.name = "TabSheet1"; t1.caption = "One";
        t2 = t1; t2.name = "TabSheet2"; t2.caption = "Two";
        p0 = lj_w_tabsheet_create(w, &t1); p1 = lj_w_tabsheet_create(w, &t2);
    }
    repaint(&root, &s);
    CHECK(lj_w_tabs_count(w) == 2 && lj_w_tabs_page(w, 1) == p1 && ((lj_w_tabs *)w)->active == 0);
    CHECK(p0->visible && !p1->visible && p0->x == 4 && p0->y == 21 + 4 && p0->w == 192 && p0->h == 150 - 21 - 8);
    CHECK(px(&s, 10 + 0, 10 + 5) == LJ_CLR_BTNHIGHLIGHT);                                       /* active tab's white left edge at x-2 */
    CHECK(px(&s, 10, 10 + 21) == LJ_CLR_BTNHIGHLIGHT && px(&s, 10 + 199, 10 + 149) == LJ_CLR_3DDKSHADOW && px(&s, 10 + 198, 10 + 148) == LJ_CLR_BTNSHADOW);   /* page frame raised */
    CHECK(px(&s, 10 + 1 + 3, 10 + 21) == LJ_CLR_BTNFACE);                                       /* frame top line broken under the active tab */
    CHECK(count_not(&s, 10 + 4, 10 + 3, 30, 14, LJ_CLR_BTNFACE) > 5);                            /* "One" */
    {
        int tw1 = lj_text_width(&lj_font_sans13, "One") + 12;
        CHECK(lj_w_tabs_at(w, 2 + tw1 + 3, 5) == 1 && lj_w_tabs_at(w, 5, 40) == -1);
        CHECK(px(&s, 10 + 2 + tw1 + 3, 10 + 1) == LJ_CLR_BTNFACE && px(&s, 10 + 2 + tw1, 10 + 5) == LJ_CLR_BTNHIGHLIGHT);   /* inactive tab starts 2 px lower, white left edge */
        click(w, 2 + tw1 + 3, 5);
    }
    CHECK(((lj_w_tabs *)w)->active == 1 && p1->visible && !p0->visible && ev_count[LJ_WEV_CHANGE] == 1 && ev_count[LJ_WEV_CHANGING] == 1);
    CHECK(key(w, LJ_VK_LEFT) && ((lj_w_tabs *)w)->active == 0 && p0->visible);
    CHECK(lj_w_tabs_select(w, 5) == -1);
    free_children(&root);
    /* ActivePage resolved lazily by name */
    d.active_page = "TabSheet2";
    w = lj_w_pagecontrol_create(&root, &d);
    {
        lj_ctl_desc t1; memset(&t1, 0, sizeof t1); t1.kind = LJ_K_TABSHEET; t1.name = "TabSheet1"; t1.caption = "One";
        p0 = lj_w_tabsheet_create(w, &t1); t1.name = "TabSheet2"; p1 = lj_w_tabsheet_create(w, &t1);
    }
    lj_widget_layout(&root);
    CHECK(((lj_w_tabs *)w)->active == 1 && p1->visible && !p0->visible);
    free_children(&root);

    /* ---- TTabControl ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_TABCONTROL; d.name = "tc"; d.left = 0; d.top = 0; d.width = 200; d.height = 100; d.items = tabs3; d.nitems = 3; d.tab_index = 1;
    w = lj_w_tabcontrol_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(lj_w_tabs_count(w) == 3 && ((lj_w_tabs *)w)->active == 1);
    click(w, 2 + lj_text_width(&lj_font_sans13, "One") + 12 + lj_text_width(&lj_font_sans13, "Two") + 12 + 3, 5);
    CHECK(((lj_w_tabs *)w)->active == 2 && ev_count[LJ_WEV_CHANGE] == 1);
    free_children(&root);

    /* ---- TMemo / TRichEdit ---- */
    for (i = 0; i < 20; i++) lines20[i] = five[i % 5];
    memset(&d, 0, sizeof d); d.kind = LJ_K_MEMO; d.name = "memo"; d.left = 10; d.top = 10; d.width = 150; d.height = 60; d.items = lines20; d.nitems = 20; d.scroll_bars = "ssVertical"; d.readonly = LJ_YES;
    w = lj_w_memo_create(&root, &d);
    repaint(&root, &s);
    CHECK(px(&s, 10, 10) == LJ_CLR_BTNSHADOW && px(&s, 12 + 148 - 2 - 16 - 10, 14) == LJ_CLR_BTNHIGHLIGHT);
    CHECK(px(&s, 10 + 150 - 2 - 16, 12) == LJ_CLR_BTNHIGHLIGHT && px(&s, 10 + 150 - 3, 12 + 15) == LJ_CLR_3DDKSHADOW);   /* scrollbar's up button */
    CHECK(count_not(&s, 15, 13, 40, 13, LJ_CLR_BTNHIGHLIGHT) > 5);
    wheel(w, -1);
    CHECK(((lj_w_memo *)w)->sb.pos == 3);
    CHECK(key(w, LJ_VK_DOWN) && ((lj_w_memo *)w)->sb.pos == 4 && key(w, LJ_VK_HOME) && ((lj_w_memo *)w)->sb.pos == 0);
    CHECK(((lj_w_memo *)w)->sb.max == 20 - 56 / 13);
    free_children(&root);
    d.kind = LJ_K_RICHEDIT; d.scroll_bars = NULL;
    w = lj_w_richedit_create(&root, &d);
    wheel(w, -1);
    CHECK(((lj_w_memo *)w)->sb.pos == 0 && !((lj_w_memo *)w)->vscroll);                          /* no scrollbar = no scrolling */
    free_children(&root);

    /* ---- TStatusBar ---- */
    {
        static const lj_column_desc panels[] = { { "Running..", 100, LJ_TA_UNSET }, { "Master 100%", 80, LJ_TA_UNSET } };
        memset(&d, 0, sizeof d); d.kind = LJ_K_STATUSBAR; d.name = "sb"; d.left = 0; d.top = 281; d.width = 400; d.height = 19; d.columns = panels; d.ncolumns = 2;
        w = lj_w_statusbar_create(&root, &d);
        CHECK(w->align == LJ_AL_BOTTOM);
        repaint(&root, &s);
        CHECK(w->y == 281 && w->w == 400);
        CHECK(px(&s, 0, 283) == LJ_CLR_BTNSHADOW && px(&s, 99, 283 + 15) == LJ_CLR_BTNHIGHLIGHT && px(&s, 100, 290) == LJ_CLR_BTNFACE && px(&s, 102, 283) == LJ_CLR_BTNSHADOW);
        CHECK(count_not(&s, 3, 284, 60, 13, LJ_CLR_BTNFACE) > 5);
        CHECK(px(&s, 396, 298) == LJ_CLR_BTNSHADOW && px(&s, 395, 298) == LJ_CLR_BTNHIGHLIGHT && px(&s, 398, 296) == LJ_CLR_BTNSHADOW);   /* grip diagonal */
        CHECK(lj_w_statusbar_set_text(w, 1, "Trig") == 0 && lj_w_statusbar_set_text(w, 99, "x") == -1);
        free_children(&root);
    }

    /* ---- TProgressBar ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_PROGRESSBAR; d.name = "pb"; d.left = 10; d.top = 10; d.width = 104; d.height = 16; d.max = 100; d.position = 50;
    w = lj_w_progressbar_create(&root, &d);
    repaint(&root, &s);
    CHECK(px(&s, 10, 10) == LJ_CLR_BTNSHADOW && px(&s, 113, 25) == LJ_CLR_BTNHIGHLIGHT);
    CHECK(count_eq(&s, 12, 12, 50, 12, LJ_CLR_HIGHLIGHT) > 100 && count_eq(&s, 62, 12, 50, 12, LJ_CLR_HIGHLIGHT) == 0);
    CHECK(px(&s, 12 + 8, 15) == LJ_CLR_BTNFACE && px(&s, 12 + 10, 15) == LJ_CLR_HIGHLIGHT);     /* 8-px blocks, 2-px gaps */
    lj_w_progressbar_set(w, 100); ((lj_w_progressbar *)w)->smooth = 1; repaint(&root, &s);
    CHECK(count_eq(&s, 12, 12, 100, 12, LJ_CLR_HIGHLIGHT) == 1200);
    free_children(&root);

    /* ---- TScrollBar ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_SCROLLBAR; d.name = "sbh"; d.left = 0; d.top = 0; d.width = 100; d.height = 16; d.max = 10; d.page_size = 1; d.kind_str = "sbHorizontal";
    w = lj_w_scrollbar_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(px(&s, 0, 0) == LJ_CLR_BTNHIGHLIGHT && px(&s, 15, 15) == LJ_CLR_3DDKSHADOW && px(&s, 99, 15) == LJ_CLR_3DDKSHADOW);
    CHECK(count_not(&s, 30, 2, 10, 2, LJ_CLR_BTNHIGHLIGHT) > 0 && count_not(&s, 30, 2, 10, 2, LJ_CLR_BTNFACE) > 0);   /* dithered track */
    click(w, 92, 8);
    CHECK(((lj_w_scrollbar *)w)->sb.pos == 1 && ev_count[LJ_WEV_SCROLL] == 1 && ev_count[LJ_WEV_CHANGE] == 1);
    CHECK(key(w, LJ_VK_RIGHT) && ((lj_w_scrollbar *)w)->sb.pos == 2 && key(w, LJ_VK_END) && ((lj_w_scrollbar *)w)->sb.pos == 10);
    click(w, 20, 8);                                                                            /* track left of the thumb: page back */
    CHECK(((lj_w_scrollbar *)w)->sb.pos == 9);
    mouse(w, LJ_EV_MOUSE_DOWN, 16 + 60, 8, 1);                                                  /* on the thumb (68 px track, 6-px thumb at 62) */
    CHECK(((lj_w_scrollbar *)w)->sb.drag);
    mouse(w, LJ_EV_MOUSE_MOVE, 16, 8, 0); mouse(w, LJ_EV_MOUSE_UP, 16, 8, 1);
    CHECK(((lj_w_scrollbar *)w)->sb.pos == 0 && !((lj_w_scrollbar *)w)->sb.drag);
    free_children(&root);
    d.kind_str = "sbVertical"; d.width = 16; d.height = 100;
    w = lj_w_scrollbar_create(&root, &d);
    CHECK(((lj_w_scrollbar *)w)->sb.vertical && key(w, LJ_VK_DOWN) && ((lj_w_scrollbar *)w)->sb.pos == 1 && !key(w, LJ_VK_RIGHT));
    free_children(&root);

    /* ---- TTrackBar ---- */
    memset(&d, 0, sizeof d); d.kind = LJ_K_TRACKBAR; d.name = "tb"; d.left = 0; d.top = 0; d.width = 120; d.height = 30; d.max = 10; d.frequency = 1; d.page_size = 2;
    w = lj_w_trackbar_create(&root, &d); bind(w);
    repaint(&root, &s);
    CHECK(lj_w_trackbar_thumb_pos(w) == 7 && px(&s, 2, 2) == LJ_CLR_BTNHIGHLIGHT);               /* thumb at Min, raised */
    CHECK(px(&s, 60, 10) == LJ_CLR_BTNSHADOW && px(&s, 60, 13) == LJ_CLR_BTNHIGHLIGHT);          /* 4-px sunken channel */
    CHECK(px(&s, 7, 2 + 21 + 2) == LJ_CLR_3DDKSHADOW && px(&s, 7 + 105, 2 + 21 + 2) == LJ_CLR_3DDKSHADOW);   /* ticks at Min and Max */
    CHECK(key(w, LJ_VK_RIGHT) && ((lj_w_trackbar *)w)->pos == 1 && ev_count[LJ_WEV_CHANGE] == 1 && lj_w_trackbar_thumb_pos(w) == 7 + 105 / 10);
    click(w, 110, 10);
    CHECK(((lj_w_trackbar *)w)->pos == 3);
    mouse(w, LJ_EV_MOUSE_DOWN, lj_w_trackbar_thumb_pos(w), 10, 1);
    CHECK(((lj_w_trackbar *)w)->drag);
    mouse(w, LJ_EV_MOUSE_MOVE, 7 + 105 * 8 / 10, 10, 0); mouse(w, LJ_EV_MOUSE_UP, 7 + 84, 10, 1);
    CHECK(((lj_w_trackbar *)w)->pos == 8 && !((lj_w_trackbar *)w)->drag);
    CHECK(key(w, LJ_VK_END) && ((lj_w_trackbar *)w)->pos == 10 && key(w, LJ_VK_PRIOR) && ((lj_w_trackbar *)w)->pos == 8);
    repaint(&root, &s);
    CHECK(px(&s, 0, 0) == LJ_CLR_3DDKSHADOW || px(&s, 1, 0) == LJ_CLR_3DDKSHADOW);              /* focus rect dots */
    free_children(&root);

    lj_surface_free(&s);
    printf("widgets_lists: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
