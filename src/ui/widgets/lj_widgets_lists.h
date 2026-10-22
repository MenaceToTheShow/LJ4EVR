/* lj_widgets_lists.h - the lists/containers widget group (design 7.2, 7.3, 7.6, 7.9):
 * TListView TListBox TCheckListBox TComboBox TTreeView TStringGrid TDrawGrid
 * TPageControl TTabSheet TTabControl THeaderControl TMemo TRichEdit TStatusBar
 * TProgressBar TScrollBar TTrackBar. One lj_w_<class>.c per class, constructors
 * declared here, state structs public so behaviour code reads/sets them directly.
 *
 * Every widget is ONE calloc (lj_w_alloc) with lj_widget as the first member:
 * free(w) releases it. Item strings are never copied: items[] point into the
 * generated tables or into memory the behaviour code owns.
 * Mouse coordinates arrive relative to the widget; a widget that starts a drag
 * (scrollbar thumb, header divider) expects the window to keep sending it
 * MOUSE_MOVE/UP until the button goes up (Delphi mouse capture). */
#ifndef LJ_UI_WIDGETS_WIDGETS_LISTS_H
#define LJ_UI_WIDGETS_WIDGETS_LISTS_H

#include <stddef.h>
#include <stdint.h>
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"
#include "ui/widgets/lj_widget_events.h"

/* ---- registration: lj_window_create consults table[kind] ---- */
typedef lj_widget *(*lj_widget_ctor)(lj_widget *parent, const lj_ctl_desc *d);
int lj_widgets_register_lists(lj_widget_ctor *table, int count);   /* fills the 17 slots of this group; -1 if count < LJ_K_COUNT */

/* ---- Windows virtual keys the group handles (lj_key_ev.key) ---- */
#define LJ_VK_RETURN   0x0D
#define LJ_VK_ESCAPE   0x1B
#define LJ_VK_SPACE    0x20
#define LJ_VK_PRIOR    0x21
#define LJ_VK_NEXT     0x22
#define LJ_VK_END      0x23
#define LJ_VK_HOME     0x24
#define LJ_VK_LEFT     0x25
#define LJ_VK_UP       0x26
#define LJ_VK_RIGHT    0x27
#define LJ_VK_DOWN     0x28
#define LJ_VK_F4       0x73
#define LJ_VK_ADD      0x6B
#define LJ_VK_SUBTRACT 0x6D

/* ---- shared helpers (lj_widgets_lists.c) ---- */
void          *lj_w_alloc(size_t size, lj_widget *parent, const lj_ctl_desc *d, const lj_widget_vt *vt, int tabstop_dflt);   /* calloc, base fields from d (class defaults of lj_form.h), font resolved, added to parent; lj_fatal on OOM */
const lj_font *lj_w_font_of(const lj_widget *w);                                        /* w->font, else the nearest ancestor's, else sans13 */
void           lj_w_text(lj_surface *s, const lj_widget *w, int x, int y, const char *str, uint32_t rgb);   /* lj_text in w's font; disabled = #808080 with white +1,+1 shadow (7.3) */
void           lj_w_emit(lj_widget *w, int ev);                                          /* w->on_event(w, ev, w->ud) when bound */
void           lj_w_arrow(lj_surface *s, int cx, int cy, int dir, uint32_t rgb);         /* classic solid triangle, 7 wide / 4 tall; dir 0 up, 1 down, 2 left, 3 right; (cx,cy) = the base's centre */
void           lj_w_checker(lj_surface *s, int x, int y, int w, int h);                  /* 50 % white / #C0C0C0 pattern (scrollbar track) */

/* An embedded classic scrollbar: 16 px arrows, dithered track, raised thumb.
 * Rect is relative to the owner widget; pos in [min, max], page = visible units. */
typedef struct { int vertical; int x, y, w, h; int min, max, pos, page; int drag, drag_off; } lj_w_sbar;
#define LJ_SB_W 16
void lj_w_sbar_paint(lj_surface *s, const lj_w_sbar *b, int ax, int ay, int enabled);    /* ax,ay = owner's absolute origin */
int  lj_w_sbar_set(lj_w_sbar *b, int pos);                                               /* clamps; 1 when pos changed */
int  lj_w_sbar_mouse(lj_w_sbar *b, const lj_mouse_ev *m);                                /* -1 outside the rect, 0 hit, 1 pos changed */
int  lj_w_sbar_key(lj_w_sbar *b, int key);                                               /* arrows/page/home/end along its axis; 1 when pos changed */
/* ME-54: the vertical bar of a scrolled list (listbox, listview, memo, string grid, tree view): at x = owner_w - edge - LJ_SB_W,
 * y, height h; n rows of which vis are visible (max = n - vis, page = vis); pos re-clamped. */
void lj_w_sbar_fit(lj_w_sbar *b, int owner_w, int edge, int y, int h, int n, int vis);
void lj_w_sbar_reveal(lj_w_sbar *b, int row, int vis);                                   /* pos moves so row is in [pos, pos + vis); row < 0: no-op */

/* ---- TListBox / TCheckListBox (lj_w_listbox.c, lj_w_checklistbox.c) and the TComboBox drop list ---- */
typedef struct {
    lj_widget base;
    const char *const *items; int n;
    int index;                 /* selected, -1 none */
    int item_h;
    lj_w_sbar sb;              /* sb.pos = first visible row */
    lj_widget *owner;          /* TComboBox when this is its drop list, else NULL */
    unsigned checks_on:1, flat:1;
    uint8_t checks[256];       /* ponytail: TCheckListBox bitset, 2048 items max; grow when a list needs more */
} lj_w_list;
lj_widget *lj_w_listbox_create(lj_widget *parent, const lj_ctl_desc *d);
lj_widget *lj_w_checklistbox_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_list_set_items(lj_widget *w, const char *const *items, int n);               /* caller owns the strings; resets index/top */
int  lj_w_list_checked(const lj_widget *w, int i);
void lj_w_list_set_checked(lj_widget *w, int i, int on);
int  lj_w_list_row_at(const lj_widget *w, int y);                                      /* item index at widget-relative y, -1 none */
void lj_w_list_paint(lj_widget *w, lj_surface *s);                                     /* the vt entries, exported for the combo drop list */
int  lj_w_list_mouse(lj_widget *w, const lj_mouse_ev *m);
int  lj_w_list_key(lj_widget *w, const lj_key_ev *k);

/* ---- TComboBox (lj_w_combobox.c) ---- */
/* Popups are separate windows (7.9): the platform installs a hook; open() hosts
 * `list` (an lj_w_list with parent NULL, geometry ax,ay,w,h absolute to the
 * combo's root) in a popup window and returns 0, or -1 to decline. Without a
 * hook, or when it declines, the list is added as the last child of the root
 * (in-client fallback, paints on top and wins the hit-test). */
typedef struct {
    int  (*open)(lj_widget *combo, lj_widget *list, int ax, int ay, void *ud);
    void (*close)(lj_widget *combo, lj_widget *list, void *ud);
    void *ud;
} lj_w_popup_hook;
typedef struct {
    lj_widget base;
    const char *const *items; int n;
    int index; int item_h, drop_count;
    const char *text;          /* csDropDown: the Text property when no item is selected */
    unsigned droplist_style:1, pressed:1, hooked:1;
    lj_widget *drop;           /* the open lj_w_list, else NULL */
    lj_w_popup_hook hook;
} lj_w_combobox;
lj_widget *lj_w_combobox_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_combobox_set_popup_hook(lj_widget *w, const lj_w_popup_hook *hook);          /* NULL = in-client fallback */
int  lj_w_combobox_drop(lj_widget *w);                                                 /* opens the list; 0 / -1 (no items or already open) */
void lj_w_combobox_close(lj_widget *w, int pick);                                      /* pick: take the list's index, fire OnChange */
void lj_w_combobox_set_items(lj_widget *w, const char *const *items, int n);

/* ---- TListView report view (lj_w_listview.c) ---- */
#define LJ_LV_COLS 32
typedef struct {
    lj_widget base;
    const lj_column_desc *cols; int ncols; int col_w[LJ_LV_COLS];
    const char *const *items; int n;      /* one string per row, '\t' separates the sub-items */
    int index; int row_h, header_h;
    int clicked_col, drag_col, drag_x;    /* OnColumnClick column; header divider drag */
    lj_w_sbar sb;
    unsigned show_headers:1, row_select:1, grid_lines:1, hide_sel:1;
} lj_w_listview;
lj_widget *lj_w_listview_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_listview_set_items(lj_widget *w, const char *const *items, int n);
int  lj_w_listview_row_at(const lj_widget *w, int y);
int  lj_w_listview_index(const lj_widget *w);                     /* ME-101: ItemIndex (-1 none, or w NULL) */
void lj_w_listview_select(lj_widget *w, int row);                  /* selects and scrolls the row into view (as a click would: OnSelectItem + OnClick when it changes) */

/* ---- THeaderControl (lj_w_headercontrol.c); the paint is shared with TListView ---- */
typedef struct {
    lj_widget base;
    const lj_column_desc *sections; int n; int widths[LJ_LV_COLS];
    int clicked, drag, drag_x, pressed;
} lj_w_headercontrol;
lj_widget *lj_w_headercontrol_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_header_paint(lj_surface *s, const lj_widget *w, int ax, int ay, int width, int h, const lj_column_desc *cols, const int *widths, int n, int pressed);
int  lj_w_header_divider_at(const int *widths, int n, int x);                          /* section whose right divider is within 3 px of x, else -1 */
int  lj_w_header_section_at(const int *widths, int n, int x);                          /* section under x, else -1 */

/* ---- TTreeView (lj_w_treeview.c): items[] with one leading '\t' per level ---- */
typedef struct {
    lj_widget base;
    const char *const *items; int n;
    int index, row_h, indent;
    lj_w_sbar sb;
    unsigned show_root:1, show_lines:1, show_buttons:1;
    uint8_t expanded[256];     /* ponytail: bitset, 2048 nodes max */
} lj_w_treeview;
lj_widget *lj_w_treeview_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_treeview_set_items(lj_widget *w, const char *const *items, int n);
int  lj_w_treeview_level(const lj_widget *w, int i);
int  lj_w_treeview_expanded(const lj_widget *w, int i);
void lj_w_treeview_expand(lj_widget *w, int i, int on);
int  lj_w_treeview_visible_count(const lj_widget *w);
int  lj_w_treeview_visible_item(const lj_widget *w, int row);                          /* item index of the row-th visible row, -1 past the end */

/* ---- TStringGrid / TDrawGrid (lj_w_stringgrid.c, lj_w_drawgrid.c) ---- */
typedef struct {
    lj_widget base;
    int rows, cols, fixed_rows, fixed_cols, def_rh, def_cw;
    int row, col;              /* the selected cell */
    int top_row;               /* first scrollable row shown */
    lj_w_sbar sb;
    unsigned row_select:1, vlines:1, hlines:1;
    int draw_col, draw_row; lj_rect draw_rect; lj_surface *draw_surf;   /* TDrawGrid: set before each OnDrawCell emit; draw_rect absolute */
    const char **cells;        /* TStringGrid: rows*cols pointers (one allocation with the struct), NULL = empty; TDrawGrid: NULL */
    const int *col_w;          /* ColWidths[]: cols entries the owner keeps; NULL = def_cw for every column */
} lj_w_grid;
lj_widget *lj_w_stringgrid_create(lj_widget *parent, const lj_ctl_desc *d);
lj_widget *lj_w_drawgrid_create(lj_widget *parent, const lj_ctl_desc *d);
lj_widget *lj_w_grid_create(lj_widget *parent, const lj_ctl_desc *d, int with_cells);  /* shared by both; with_cells = TStringGrid storage */
int  lj_w_grid_set_cell(lj_widget *w, int col, int row, const char *text);              /* -1 out of range or TDrawGrid */
int  lj_w_grid_cell_at(const lj_widget *w, int x, int y, int *col, int *row);            /* -1 outside the cells */
int  lj_w_grid_cell_rect(const lj_widget *w, int col, int row, lj_rect *out);            /* widget-relative; -1 when not shown */

/* ---- TPageControl + TTabSheet, TTabControl (lj_w_pagecontrol.c, lj_w_tabsheet.c, lj_w_tabcontrol.c) ---- */
#define LJ_TAB_PAD 4           /* page inset inside the frame */
typedef struct {
    lj_widget base;
    int active;                /* TPageControl: index among the TTabSheet children; TTabControl: Tabs index */
    int tab_h;
    const char *const *tabs; int ntabs;   /* TTabControl only */
} lj_w_tabs;
typedef struct { lj_widget base; const char *caption; } lj_w_tabsheet;
lj_widget *lj_w_pagecontrol_create(lj_widget *parent, const lj_ctl_desc *d);
lj_widget *lj_w_tabsheet_create(lj_widget *parent, const lj_ctl_desc *d);
lj_widget *lj_w_tabcontrol_create(lj_widget *parent, const lj_ctl_desc *d);
int        lj_w_tabs_count(const lj_widget *w);
lj_widget *lj_w_tabs_page(const lj_widget *w, int i);                                   /* TPageControl: the i-th TTabSheet child, NULL otherwise */
int        lj_w_tabs_select(lj_widget *w, int i);                                       /* activates, shows that page, fires OnChange; -1 out of range */
int        lj_w_tabs_at(const lj_widget *w, int x, int y);                              /* tab under widget-relative x,y, -1 none */
void       lj_w_tabs_paint(lj_widget *w, lj_surface *s);                                /* shared vt entries */
int        lj_w_tabs_mouse(lj_widget *w, const lj_mouse_ev *m);
int        lj_w_tabs_key(lj_widget *w, const lj_key_ev *k);
void       lj_w_tabs_layout(lj_widget *w);

/* ---- TMemo / TRichEdit, read-only plain text (lj_w_memo.c, lj_w_richedit.c) ---- */
typedef struct {
    lj_widget base;
    const char *const *lines; int n;
    int line_h;
    lj_w_sbar sb;
    unsigned vscroll:1, border:1;
} lj_w_memo;
lj_widget *lj_w_memo_create(lj_widget *parent, const lj_ctl_desc *d);
lj_widget *lj_w_richedit_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_memo_set_lines(lj_widget *w, const char *const *lines, int n);
void lj_w_memo_paint(lj_widget *w, lj_surface *s);
int  lj_w_memo_mouse(lj_widget *w, const lj_mouse_ev *m);
int  lj_w_memo_key(lj_widget *w, const lj_key_ev *k);

/* ---- TStatusBar (lj_w_statusbar.c): panels from desc->columns (caption = Text, width), SimplePanel = one panel ---- */
#define LJ_SB_PANELS 16
typedef struct {
    lj_widget base;
    const lj_column_desc *panels; int n;
    const char *text[LJ_SB_PANELS];       /* runtime panel texts; NULL = the desc caption; text[0] serves SimpleText */
    unsigned simple:1, grip:1;
} lj_w_statusbar;
lj_widget *lj_w_statusbar_create(lj_widget *parent, const lj_ctl_desc *d);
int  lj_w_statusbar_set_text(lj_widget *w, int panel, const char *text);               /* -1 out of range */

/* ---- TProgressBar, TScrollBar, TTrackBar ---- */
typedef struct { lj_widget base; int min, max, pos, step; unsigned smooth:1, vertical:1; } lj_w_progressbar;
lj_widget *lj_w_progressbar_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_progressbar_set(lj_widget *w, int pos);

typedef struct { lj_widget base; lj_w_sbar sb; } lj_w_scrollbar;
lj_widget *lj_w_scrollbar_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_scrollbar_set(lj_widget *w, int pos);                                        /* clamps, repaints, no event */

typedef struct { lj_widget base; int min, max, pos, freq, page; int drag; unsigned vertical:1, ticks_both:1, ticks_none:1; } lj_w_trackbar;
lj_widget *lj_w_trackbar_create(lj_widget *parent, const lj_ctl_desc *d);
void lj_w_trackbar_set(lj_widget *w, int pos);                                         /* clamps, repaints, no event */
int  lj_w_trackbar_thumb_pos(const lj_widget *w);                                      /* thumb centre along the axis, widget-relative */

#endif /* LJ_UI_WIDGETS_WIDGETS_LISTS_H */
