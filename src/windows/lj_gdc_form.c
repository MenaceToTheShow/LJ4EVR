/* lj_gdc_form.c - see lj_gdc_form.h. Four windows, each binds every OnXxx its DFM declares (ME-16):
 *  TGenericMegaForm "Generic DMX Control" (+ TMegaGridSizeForm, TMegaGridColorSchemeForm, TMegaGroupCreateForm),
 *  TDefault2Form "Set Fade Status" (Sequence fades), TFollowSpotForm "Edit FollowSpot Settings", TFollowListForm.
 * Every scene write goes through the model (lj_gdc_run, lj_seqfades_apply, lj_followspot_record) under lj_lock on the
 * current scene, then LJ_CMD_CURRENT_SEQ (docs/w3-contracts.md seam 5). Follow spot activation is the app's
 * (LJ_ACT_FOLLOWSPOT_APPLY). */
#include "windows/lj_gdc_form.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "model/lj_followspot.h"
#include "model/lj_gdc.h"
#include "model/lj_seqfades.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_w_speedbutton.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#define LJ_WIN_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */
#define GDC_CAPTION "Generic DMX Control"
#define FORMS_SEC "GENERIC"     /* forms.ini section, exe 0x1E5AC4 */

static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static const char *menu_item_name(lj_window *win) { const lj_menu_item_desc *it = win ? lj_window_menu_item(win) : NULL; return it && it->name ? it->name : ""; }
static void set_caption(lj_window *win, const char *name, const char *text) { lj_widget *w = win ? lj_window_find(win, name) : NULL; if (w) { lj_w_set_caption(w, text); lj_widget_invalidate(w); } }
static void set_enabled(lj_window *win, const char *name, int on) { lj_widget *w = win ? lj_window_find(win, name) : NULL; if (w && (int)w->enabled != (on != 0)) { w->enabled = on != 0; lj_widget_invalidate(w); } }
static int mouse_in(lj_window *win, lj_widget *w, int *x, int *y)    /* the last mouse event, widget-relative */
{
    const lj_mouse_ev *m = lj_window_last_event(win);
    int ax, ay;
    if (!m || !w) return -1;
    lj_widget_abs(w, &ax, &ay);                                          /* surface coordinates: the root's own x,y is in the sum */
    *x = m->x - ax; *y = m->y - ay;
    return 0;
}
static void popup_at(lj_window *win, const char *menu, int x, int y)    /* a TPopupMenu the DFM does not attach (LJ2 shows it from OnMouseDown) */
{
    lj_menu *m = lj_window_menus(win);
    int sx = 0, sy = 0;
    if (!m) return;
    if (m->host && m->host->screen_pos) m->host->screen_pos(m->host->ud, win, &sx, &sy);
    lj_menu_popup(m, lj_form_find_menu(lj_window_desc(win), menu), sx + x, sy + y);
}
/* TListView.OnChange: our list view emits OnSelectItem / OnClick on a new row, not OnChange; the row change reaches the
 * DFM's lv1Change handler through this wrapper (ponytail: one hook per list, until lj_w_listview emits LJ_WEV_CHANGE). */
typedef struct { void (*prev)(lj_widget *, int, void *); void *ud; void (*change)(lj_widget *, void *); } lv_hook;
static void lv_hook_event(lj_widget *w, int ev, void *ud)
{
    lv_hook *h = (lv_hook *)ud;
    if (h->prev) h->prev(w, ev, h->ud);
    if (ev == LJ_WEV_SELECT_ITEM) h->change(w, NULL);
}
static void lv_hook_install(lv_hook *h, lj_widget *lv, void (*change)(lj_widget *, void *))
{
    if (!lv) return;
    h->prev = lv->on_event; h->ud = lv->ud; h->change = change;
    lv->on_event = lv_hook_event; lv->ud = h;
}
static lj_scene *cur_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = c->show ? c->show->current_seq : NULL;
    return s && s->scenes && c->current_scene < s->n ? &s->scenes[c->current_scene] : NULL;
}
static void scene_written(void) { lj_seq_form_mark_modified(); lj_seq_form_post_current(); }
static int lib_path(const char *rel, char *out, size_t cap) { lj_ctx *c = lj_ctx_get(); return c->lib ? lj_library_path(c->lib, rel, out, cap) : -1; }

/* ================= Generic DMX Control (TGenericMegaForm) ================= */

enum { GC_BACK = 0, GC_CHANNEL, GC_SELECTED, GC_OFF, GC_SNAP, GC_FADE, GC_COUNT };
static const char *const gc_keys[GC_COUNT] = { "GridBackColor", "ChanelColor", "SelectedColor", "OffColor", "SnapColor", "FadeColor" };   /* forms.ini keys verbatim (exe 0x1E5B7C..) */
static const uint32_t gc_default[GC_COUNT] = { 0x808080u /* clGray */, 0x000080u /* clNavy */, 0xFF9933u /* orange, measured from genericcontrol.gif (reduced), PROPOSAL rig F35 */,
                                               0xFF0000u, 0xFFFF00u, 0x00FF00u /* clRed clYellow clLime: pOff pSnap pFade Color */ };
enum { GP_VIEW_GROUP = 0, GP_VIEW_FADER, GP_PCT, GP_AUTOSET, GP_RECT, GP_AUTOSCROLL, GP_CMDLINE, GP_LOCKKEYS, GP_EMPTYCLEAR, GP_COUNT };
static const struct { const char *item, *key; int dflt; } gp_rows[GP_COUNT] = {   /* PrefsClick items; key NULL = not persisted (no exe key) */
    { "M_ViewGroup", "ViewGroups", 1 }, { "M_ViewLevelFader", "ViewLevelFader", 1 }, { "M_UsePctValues", "PctValues", 1 /* genericcontrol.gif shows % */ },
    { "M_AutosetMatrixSize", NULL, 1 }, { "M_RectSelect", NULL, 1 }, { "M_AutoScroll", "AutoScroll", 0 },
    { "M_ShowCommandLine", "ShowCommandLine", 1 }, { "M_LockCursorKeys", "LockCursorKeys", 0 }, { "M_EmptyClear", "EmptyClear", 1 } };   /* defaults PROPOSAL rig F35 */

static struct {
    lj_window *win, *size_win, *color_win, *create_win;
    lj_widget *grid, *fader, *cmd;
    lj_gdc_state own;
    int pref[GP_COUNT]; uint32_t color[GC_COUNT]; int cw, rh;
    int code[LJ_CHANS], ncode, cols;                /* visible cells: mapped channels in address order */
    int level, last_level, fader_down, rel;         /* the level fader in display units */
    int down, anchor, pop_cell, create_rename;
    char last[LJ_GDC_LINE_MAX], err[96];
    int nrows; int row_group[LJ_GEN_GROUPS]; char rows[LJ_GEN_GROUPS][48]; const char *items[LJ_GEN_GROUPS];
    int groups_loaded;
    lj_widget_vt cmd_vt, grid_vt;
    lv_hook lv_hook;
    int relaid;
} g;

static lj_gdc_state *gst(void) { lj_ctx *c = lj_ctx_get(); return c->gdc ? c->gdc : &g.own; }
static int unit_max(void) { return g.pref[GP_PCT] ? 100 : 255; }

static void groups_save(void)
{
    char path[LJ_PATH_CAP];
    lj_ctx *c = lj_ctx_get();
    if (c->show && lib_path(LJ_GDC_GROUPS_FILE, path, sizeof path) == 0 && lj_gdc_groups_save(path, &c->show->gdc) != 0) lj_log("generic dmx: cannot save %s", path);
}

static void caption_update(void)
{
    lj_ctx *c = lj_ctx_get();
    char t[96];
    int ch, hidden = 0, sel = 0;
    lj_gdc_state *st = gst();
    if (!g.win) return;
    for (ch = 0; ch < LJ_CHANS; ch++) { if (c->show && c->show->gdc.hidden[ch] && lj_gdc_is_generic(c->show, ch)) hidden++; sel += st->sel[ch] != 0; }
    if (hidden) snprintf(t, sizeof t, GDC_CAPTION " (%d Channels Hidden)", hidden); else lj_str_copy(t, sizeof t, GDC_CAPTION);   /* exe 0x1E5258 " (", 0x1E5264 " Channels Hidden)" */
    lj_window_set_caption(g.win, t);
    snprintf(t, sizeof t, "%d Channels", sel);                                                          /* pSelected, exe 0x1E778C " Channels" */
    set_caption(g.win, "pSelected", t);
}

static void cells_build(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_w_grid *gr = (lj_w_grid *)g.grid;
    int ch, avail;
    g.ncode = 0;
    for (ch = 0; c->show && ch < LJ_CHANS; ch++) if (lj_gdc_is_generic(c->show, ch) && !c->show->gdc.hidden[ch]) g.code[g.ncode++] = ch;
    if (!gr) return;
    avail = g.grid->w - 18;                                                                             /* the vertical scroll bar */
    if (g.pref[GP_AUTOSET] || g.cols < 1) g.cols = avail / (g.cw > 0 ? g.cw : 32);
    if (g.cols < 1) g.cols = 1;
    gr->cells = NULL; gr->fixed_rows = gr->fixed_cols = 0; gr->row_select = 0;
    gr->def_cw = g.cw; gr->def_rh = g.rh;
    gr->cols = g.cols; gr->rows = g.ncode ? (g.ncode + g.cols - 1) / g.cols : 1;
    lj_widget_invalidate(g.grid);
    caption_update();
}

static void groups_list(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *lv = g.win ? lj_window_find(g.win, "lv1") : NULL;
    int i;
    g.nrows = 0;
    for (i = 0; c->show && i < LJ_GEN_GROUPS; i++) {
        if (!c->show->gdc.used[i]) continue;
        snprintf(g.rows[g.nrows], sizeof g.rows[g.nrows], "%d\t%s", i + 1, c->show->gdc.name[i]);
        g.row_group[g.nrows] = i; g.items[g.nrows] = g.rows[g.nrows]; g.nrows++;
    }
    if (lv) lj_w_listview_set_items(lv, g.items, g.nrows);
}

/* the one write path: a program against the current scene under the lock */
static int run_prog(const lj_gdc_prog *p)
{
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc;
    int n = -1;
    lj_gdc_state *st = gst();
    st->percent = (uint8_t)g.pref[GP_PCT]; st->empty_clears = (uint8_t)g.pref[GP_EMPTYCLEAR];
    if (!c->show) return -1;
    lj_lock();
    if ((sc = cur_scene()) != NULL) n = lj_gdc_run(p, c->show, st, sc);
    lj_unlock();
    if (n > 0) scene_written();
    if (g.grid) lj_widget_invalidate(g.grid);
    caption_update();
    return n;
}
static void run1(int op, int lo, int hi, int v) { lj_gdc_prog p; p.n = 1; p.c[0].op = (uint8_t)op; p.c[0].lo = (uint16_t)lo; p.c[0].hi = (uint16_t)hi; p.c[0].v = (uint16_t)v; run_prog(&p); }

int lj_gdc_form_command(const char *line)
{
    lj_gdc_prog p;
    if (lj_gdc_parse(line, &p, g.err, sizeof g.err) != 0) { lj_log("generic dmx: \"%s\": %s", line, g.err); return -1; }   /* the line stays for correction (LJ2's reaction is unrecorded: PROPOSAL rig F35) */
    if (line && line[0]) lj_str_copy(g.last, sizeof g.last, line);                                                    /* Recall: "the last non-empty command line" */
    if (g.cmd) lj_w_edit_set_text(g.cmd, "");                                                                         /* "the command line is automatically cleared" */
    return run_prog(&p) < 0 ? -1 : 0;
}

static int rel_mode(void) { lj_widget *rb = g.win ? lj_window_find(g.win, "sbRel") : NULL; return rb && lj_w_speedbutton_down(rb); }   /* sbAbs / sbRel: GroupIndex 1 */
static void fader_set(int v)                    /* the level fader moved: absolute = the level, relative = the change */
{
    int max = unit_max();
    g.rel = rel_mode();
    if (v < 0) v = 0;
    if (v > max) v = max;
    if (v == g.level && !g.rel) { run1(LJ_GDC_LEVEL, 0, 0, v); return; }
    if (g.rel) { int d = v - g.level; if (d) run1(d > 0 ? LJ_GDC_LEVEL_UP : LJ_GDC_LEVEL_DOWN, 0, 0, d > 0 ? d : -d); }
    else run1(LJ_GDC_LEVEL, 0, 0, v);
    g.level = v;
    if (g.fader) lj_widget_invalidate(g.fader);
}

/* ---- the command line: [Enter] runs, the cursor keys drive the fader (CHM "Keyboard shortcuts") ---- */
static int cmd_key(lj_widget *w, const lj_key_ev *k)
{
    if (k->type == LJ_EV_KEY_DOWN) {
        switch (k->key) {
        case LJ_VK_RETURN: lj_gdc_form_command(lj_w_edit_text(w)); return 1;
        case LJ_VK_UP: fader_set(g.level + 1); return 1;
        case LJ_VK_DOWN: fader_set(g.level - 1); return 1;
        case LJ_VK_PRIOR: fader_set(g.level + 10); return 1;
        case LJ_VK_NEXT: fader_set(g.level - 1); return 1;    /* CHM: "[Page Dn]: decreases level by 1 unit" (sic, kept) */
        case LJ_VK_HOME: fader_set(unit_max()); return 1;
        case LJ_VK_END: fader_set(0); return 1;
        default: break;
        }
    }
    return lj_w_edit_vt.key ? lj_w_edit_vt.key(w, k) : 0;
}

/* ---- the grid ---- */
static int cell_code(int col, int row) { int i = row * g.cols + col; return col >= 0 && col < g.cols && row >= 0 && i < g.ncode ? g.code[i] : -1; }
static int (*lj_w_grid_vt_mouse)(lj_widget *, const lj_mouse_ev *);   /* the class's own mouse entry, kept by grid_mouse */
static void on_draw_cell(lj_widget *w, void *ud)
{
    lj_w_grid *gr = (lj_w_grid *)w;
    lj_surface *s = gr->draw_surf;
    const lj_font *f = lj_w_font_of(w);
    lj_scene *sc = cur_scene();
    int code = cell_code(gr->draw_col, gr->draw_row), x = gr->draw_rect.x, y = gr->draw_rect.y, v = 0, st = LJ_OFF;
    char t[16];
    (void)ud;
    if (!s) return;
    if (code < 0) { lj_fill(s, x, y, gr->draw_rect.w, gr->draw_rect.h, LJ_CLR_BTNFACE); return; }
    if (sc) { v = sc->val[code]; st = sc->state[code]; }
    lj_fill(s, x, y, gr->draw_rect.w, gr->draw_rect.h, gst()->sel[code] ? g.color[GC_SELECTED] : g.color[GC_BACK]);
    snprintf(t, sizeof t, "%d", code + 1);
    lj_text(s, f, x + (gr->draw_rect.w - lj_text_width(f, t)) / 2, y + 1, t, g.color[GC_CHANNEL], 0);
    if (g.pref[GP_PCT]) snprintf(t, sizeof t, "%d%%", (v * 100 + 127) / 255); else snprintf(t, sizeof t, "%d", v);
    lj_text(s, f, x + (gr->draw_rect.w - lj_text_width(f, t)) / 2, y + gr->draw_rect.h - f->height - 1, t, g.color[st == LJ_SNAP ? GC_SNAP : st == LJ_FADE ? GC_FADE : GC_OFF], 0);
}
static int grid_cell_at_mouse(int *col, int *row) { int x, y; return mouse_in(g.win, g.grid, &x, &y) || lj_w_grid_cell_at(g.grid, x, y, col, row) ? -1 : 0; }
static void select_rect(int a, int b, int add)   /* cells a..b as a rectangle (Mouse Select Rectangles) or a range */
{
    lj_gdc_state *st = gst();
    int c0 = a % g.cols, r0 = a / g.cols, c1 = b % g.cols, r1 = b / g.cols, r, cc, t;
    if (!add) memset(st->sel, 0, LJ_CHANS);
    if (c0 > c1) { t = c0; c0 = c1; c1 = t; }
    if (r0 > r1) { t = r0; r0 = r1; r1 = t; }
    for (r = r0; r <= r1; r++) for (cc = c0; cc <= c1; cc++) { int code = cell_code(cc, r); if (code >= 0) st->sel[code] = 1; }
}
static void on_grid_down(lj_widget *w, void *ud)
{
    const lj_mouse_ev *m = lj_window_last_event(g.win);
    int col, row, i;
    (void)w; (void)ud;
    if (!m || grid_cell_at_mouse(&col, &row) || cell_code(col, row) < 0) return;
    i = row * g.cols + col;
    if (m->button == 2) { g.pop_cell = i; return; }                       /* GridPop (Select Row / Deselect Row) */
    if (m->ctrl) { gst()->sel[g.code[i]] ^= 1; }
    else select_rect(i, i, m->shift);
    g.down = 1; g.anchor = i;
    lj_widget_invalidate(g.grid); caption_update();
}
static void on_grid_move(lj_widget *w, void *ud)
{
    int col, row, i;
    (void)w; (void)ud;
    if (!g.down || grid_cell_at_mouse(&col, &row) || cell_code(col, row) < 0) return;
    i = row * g.cols + col;
    if (g.pref[GP_RECT]) select_rect(g.anchor, i, 0); else gst()->sel[g.code[i]] = 1;   /* rectangles, else "painting" cells */
    lj_widget_invalidate(g.grid); caption_update();
}
static void on_grid_up(lj_widget *w, void *ud) { (void)w; (void)ud; g.down = 0; }
static void on_grid_pop(lj_widget *w, void *ud) { (void)w; (void)ud; }
/* our TDrawGrid emits OnDrawCell / OnSelectCell / OnClick but not the DG1MouseDown/Move/Up the DFM binds; the grid's
 * mouse entry forwards them (ponytail: per-window vt copy, until lj_w_grid emits LJ_WEV_MOUSE_*) */
static int grid_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    if (m->type == LJ_EV_MOUSE_DOWN) on_grid_down(w, NULL);
    else if (m->type == LJ_EV_MOUSE_MOVE) on_grid_move(w, NULL);
    else if (m->type == LJ_EV_MOUSE_UP) on_grid_up(w, NULL);
    return lj_w_grid_vt_mouse ? lj_w_grid_vt_mouse(w, m) : 1;
}
static void on_pop_click(lj_widget *w, void *ud)                         /* P_SelectRow / P_DeselectRow */
{
    const char *n = menu_item_name(g.win);
    int row = g.cols ? g.pop_cell / g.cols : 0, cc, on = strcmp(n, "P_SelectRow") == 0;
    (void)w; (void)ud;
    for (cc = 0; cc < g.cols; cc++) { int code = cell_code(cc, row); if (code >= 0) gst()->sel[code] = (uint8_t)on; }
    lj_widget_invalidate(g.grid); caption_update();
}

/* ---- the level fader pb1 and its buttons ---- */
static void on_fader_paint(lj_widget *w, void *ud)
{
    lj_surface *s = lj_window_surface(g.win);
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, h, max = unit_max();
    char t[16];
    (void)ud;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, 0x000000u);                             /* black well, blue level, value below: genericcontrol.gif (reduced), PROPOSAL rig F35 */
    h = (w->h - f->height - 4) * g.level / max;
    lj_fill(s, ax + 2, ay + w->h - f->height - 2 - h, w->w - 4, h, 0x0000FFu);
    if (g.pref[GP_PCT]) snprintf(t, sizeof t, "%d%%", g.level); else snprintf(t, sizeof t, "%d", g.level);
    lj_text(s, f, ax + (w->w - lj_text_width(f, t)) / 2, ay + w->h - f->height - 1, t, LJ_CLR_BTNHIGHLIGHT, 0);
}
static void fader_mouse(void)
{
    int x, y, span = g.fader->h - lj_w_font_of(g.fader)->height - 4;
    if (mouse_in(g.win, g.fader, &x, &y) || span <= 0) return;
    fader_set((span - (y - 2)) * unit_max() / span);
}
static void on_fader_down(lj_widget *w, void *ud) { (void)w; (void)ud; g.fader_down = 1; fader_mouse(); }
static void on_fader_move(lj_widget *w, void *ud) { (void)w; (void)ud; if (g.fader_down) fader_mouse(); }
static void on_fader_up(lj_widget *w, void *ud) { (void)w; (void)ud; g.fader_down = 0; }
static void on_sb_down(lj_widget *w, void *ud) { (void)ud; fader_set(g.level + (w->name && !strcmp(w->name, "sbFaderUp") ? 1 : -1)); }   /* sbFaderUp / sbFaderDown: one unit */
static void on_state_down(lj_widget *w, void *ud)                        /* pOff / pSnap / pFade / pRecall share the handler */
{
    (void)ud;
    if (!w->name) return;
    if (!strcmp(w->name, "pRecall")) { if (g.cmd) lj_w_edit_set_text(g.cmd, g.last); return; }
    run1(LJ_GDC_STATE, 0, 0, !strcmp(w->name, "pSnap") ? LJ_SNAP : !strcmp(w->name, "pFade") ? LJ_FADE : LJ_OFF);
}
static void on_clear_sel(lj_widget *w, void *ud) { (void)w; (void)ud; memset(gst()->sel, 0, LJ_CHANS); if (g.grid) lj_widget_invalidate(g.grid); caption_update(); }   /* SpeedButton3 "Clear" */

/* ---- groups ---- */
static int lv_group(void) { lj_widget *lv = g.win ? lj_window_find(g.win, "lv1") : NULL; int i = lv ? ((lj_w_listview *)lv)->index : -1; return i >= 0 && i < g.nrows ? g.row_group[i] : -1; }
static void on_lv_change(lj_widget *w, void *ud)                         /* a highlighted group selects its channels */
{
    int gi = lv_group();
    (void)w; (void)ud;
    if (gi >= 0) run1(LJ_GDC_GRP_SET, gi + 1, gi + 1, 0);
}
static void on_lv_down(lj_widget *w, void *ud)
{
    const lj_mouse_ev *m = lj_window_last_event(g.win);
    int x, y;
    (void)ud;
    if (m && m->button == 2 && !mouse_in(g.win, w, &x, &y)) { int ax, ay; lj_widget_abs(w, &ax, &ay); popup_at(g.win, "GroupPop", ax + x, ay + y); }
}
static void on_group_pop(lj_widget *w, void *ud)
{
    lj_menu *m = lj_window_menus(g.win);
    int gi = lv_group(), i;
    (void)w; (void)ud;
    for (i = 0; m && i < 7; i++) {
        static const char *const it[] = { "P_RenameGroup", "P_DeleteGroup", "P_AddSelectedtoGroup", "P_RemoveSelectedFromGroup", "P_SetGroup", "P_ClearGroup", "P_CreateGroup" };
        lj_menu_set_enabled(m, it[i], i == 6 || gi >= 0);
    }
}
static void group_edit(int gi, int how)          /* 0 add selected, 1 remove selected, 2 set to selected, 3 clear */
{
    lj_ctx *c = lj_ctx_get();
    lj_gdc_state *st = gst();
    uint32_t bit = 1u << gi;
    int ch;
    if (!c->show || gi < 0) return;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int s = st->sel[ch] != 0;
        if (how == 0 && s) c->show->gdc.member[ch] |= bit;
        else if (how == 1 && s) c->show->gdc.member[ch] &= ~bit;
        else if (how == 2) c->show->gdc.member[ch] = s ? c->show->gdc.member[ch] | bit : c->show->gdc.member[ch] & ~bit;
        else if (how == 3) c->show->gdc.member[ch] &= ~bit;
    }
    groups_save();
}
static void on_add_sel(lj_widget *w, void *ud) { (void)w; (void)ud; group_edit(lv_group(), 0); }
static void on_rem_sel(lj_widget *w, void *ud) { (void)w; (void)ud; group_edit(lv_group(), 1); }
static void on_set_group(lj_widget *w, void *ud) { (void)w; (void)ud; group_edit(lv_group(), 2); }
static void clear_group_result(int yes, void *ud) { if (yes) group_edit((int)(intptr_t)ud, 3); }
static void on_clear_group(lj_widget *w, void *ud) { int gi = lv_group(); (void)w; (void)ud; if (gi >= 0) lj_win_confirm(GDC_CAPTION, "Clear Group ?", "", clear_group_result, (void *)(intptr_t)gi); }   /* exe 0x1E7ED0 */
static void delete_group_result(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    int gi = (int)(intptr_t)ud;
    if (!yes || !c->show) return;
    group_edit(gi, 3);
    c->show->gdc.used[gi] = 0; c->show->gdc.name[gi][0] = 0;
    groups_save(); groups_list();
}
static void on_delete_group(lj_widget *w, void *ud) { int gi = lv_group(); (void)w; (void)ud; if (gi >= 0) lj_win_confirm(GDC_CAPTION, "Delete Selected Group ?", "", delete_group_result, (void *)(intptr_t)gi); }   /* exe 0x1E7E58 */
static void all_groups(int del)
{
    lj_ctx *c = lj_ctx_get();
    if (!c->show) return;
    memset(c->show->gdc.member, 0, sizeof c->show->gdc.member);
    if (del) { memset(c->show->gdc.used, 0, sizeof c->show->gdc.used); memset(c->show->gdc.name, 0, sizeof c->show->gdc.name); }
    groups_save(); groups_list();
}
static void on_delete_all(lj_widget *w, void *ud) { (void)w; (void)ud; all_groups(1); }
static void on_clear_all(lj_widget *w, void *ud) { (void)w; (void)ud; all_groups(0); }

/* TMegaGroupCreateForm "Create Channel Group": eNo (read-only number), eName; bkOK / bkCancel carry no OnClick */
static void create_close(void) { if (g.create_win) { lj_win_hide(g.create_win); lj_window_destroy(g.create_win); g.create_win = NULL; } }
static void create_ok(lj_widget *w, int ev, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const char *no = lj_w_edit_text(lj_window_find(g.create_win, "eNo")), *name = lj_w_edit_text(lj_window_find(g.create_win, "eName"));
    int gi = no ? atoi(no) - 1 : -1;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (c->show && gi >= 0 && gi < LJ_GEN_GROUPS) { c->show->gdc.used[gi] = 1; lj_str_copy(c->show->gdc.name[gi], sizeof c->show->gdc.name[gi], name ? name : ""); groups_save(); }
    create_close();
    groups_list();
}
static void create_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) create_close(); }
static void create_open(int gi)
{
    static const lj_binding b[] = { { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { NULL, NULL, NULL } };
    lj_ctx *c = lj_ctx_get();
    lj_widget *w;
    char t[12];   /* any int (gcc -Wformat-truncation) */
    if (gi < 0 || gi >= LJ_GEN_GROUPS) { lj_log("generic dmx: all %d groups are in use", LJ_GEN_GROUPS); return; }
    create_close();
    if ((g.create_win = lj_window_create(&lj_form_TMEGAGROUPCREATEFORM, b)) == NULL) return;
    snprintf(t, sizeof t, "%d", gi + 1);
    if ((w = lj_window_find(g.create_win, "eNo")) != NULL) { lj_w_edit_set_text(w, t); ((lj_w_edit *)w)->readonly = 1; }
    if ((w = lj_window_find(g.create_win, "eName")) != NULL && c->show) lj_w_edit_set_text(w, c->show->gdc.name[gi]);
    if ((w = lj_window_find(g.create_win, "bOk")) != NULL) { w->on_event = create_ok; w->ud = NULL; }
    if ((w = lj_window_find(g.create_win, "BitBtn2")) != NULL) { w->on_event = create_cancel; w->ud = NULL; }
    lj_win_show(g.create_win, LJ_WIN_STAY_ON_TOP);
}
static void on_create_group(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    int i;
    (void)w; (void)ud;
    for (i = 0; c->show && i < LJ_GEN_GROUPS && c->show->gdc.used[i]; i++) { }
    create_open(c->show ? i : -1);
}
static void on_rename_group(lj_widget *w, void *ud) { (void)w; (void)ud; if (lv_group() >= 0) create_open(lv_group()); }

/* ---- Options / Preferences ---- */
static void hide_channels(int hide)
{
    lj_ctx *c = lj_ctx_get();
    int ch;
    if (!c->show) return;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        if (!hide) c->show->gdc.hidden[ch] = 0;
        else if (gst()->sel[ch]) { c->show->gdc.hidden[ch] = 1; gst()->sel[ch] = 0; }
    }
    groups_save(); cells_build();
}
static void on_hide(lj_widget *w, void *ud) { (void)w; (void)ud; hide_channels(1); }
static void on_show_all(lj_widget *w, void *ud) { (void)w; (void)ud; hide_channels(0); }

static void prefs_apply(void)
{
    lj_menu *m = g.win ? lj_window_menus(g.win) : NULL;
    lj_widget *w;
    int i;
    for (i = 0; m && i < GP_COUNT; i++) lj_menu_set_checked(m, gp_rows[i].item, g.pref[i]);
    if ((w = lj_window_find(g.win, "Panel7")) != NULL) w->visible = (unsigned)g.pref[GP_VIEW_GROUP];
    if ((w = lj_window_find(g.win, "Panel6")) != NULL) w->visible = (unsigned)g.pref[GP_VIEW_FADER];
    if ((w = lj_window_find(g.win, "Panel1")) != NULL) w->visible = (unsigned)(g.pref[GP_VIEW_GROUP] || g.pref[GP_VIEW_FADER]);   /* neither: the grid fills the control */
    if ((w = lj_window_find(g.win, "Panel2")) != NULL) w->visible = (unsigned)g.pref[GP_CMDLINE];
    if (!g.pref[GP_VIEW_GROUP] || !g.pref[GP_VIEW_FADER] || !g.pref[GP_CMDLINE] || g.relaid) {   /* the DFM rects stand while every panel shows (W0 rule; the alBottom order needs ME-100) */
        lj_widget_layout(lj_window_root(g.win));
        g.relaid = 1;
    }
    lj_window_invalidate_all(g.win);
    cells_build();
}
static void on_prefs(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const char *n = menu_item_name(g.win);
    int i;
    (void)w; (void)ud;
    for (i = 0; i < GP_COUNT; i++) if (!strcmp(n, gp_rows[i].item)) {
        g.pref[i] = !g.pref[i];
        if (gp_rows[i].key && c->forms_ini) lj_ini_seti(c->forms_ini, FORMS_SEC, gp_rows[i].key, g.pref[i]);
        if (i == GP_PCT) g.level = g.pref[i] ? (g.level * 100 + 127) / 255 : (g.level * 255 + 50) / 100;
    }
    prefs_apply();
}

/* TMegaGridSizeForm "Set Grid Size": HSize / VSize (CellWidth / CellHight) */
static void size_close(lj_widget *w, void *ud) { (void)w; (void)ud; if (g.size_win) { lj_win_hide(g.size_win); lj_window_destroy(g.size_win); g.size_win = NULL; } }
static void on_size_change(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)ud;
    if (!w->name) return;
    if (!strcmp(w->name, "HSize")) g.cw = ((lj_w_trackbar *)w)->pos; else g.rh = ((lj_w_trackbar *)w)->pos;
    if (c->forms_ini) { lj_ini_seti(c->forms_ini, FORMS_SEC, "CellWidth", g.cw); lj_ini_seti(c->forms_ini, FORMS_SEC, "CellHight", g.rh); }
    cells_build();
}
static void on_grid_size(lj_widget *w, void *ud)
{
    static const lj_binding b[] = { { "FormClose", size_close, NULL }, { "SizeChange", on_size_change, NULL }, { NULL, NULL, NULL } };
    lj_widget *t;
    (void)w; (void)ud;
    if (g.size_win || (g.size_win = lj_window_create(&lj_form_TMEGAGRIDSIZEFORM, b)) == NULL) return;
    if ((t = lj_window_find(g.size_win, "HSize")) != NULL) { ((lj_w_trackbar *)t)->min = 16; ((lj_w_trackbar *)t)->max = 64; lj_w_trackbar_set(t, g.cw); }   /* ranges PROPOSAL rig F35 */
    if ((t = lj_window_find(g.size_win, "VSize")) != NULL) { ((lj_w_trackbar *)t)->min = 16; ((lj_w_trackbar *)t)->max = 48; lj_w_trackbar_set(t, g.rh); }
    lj_win_show(g.size_win, LJ_WIN_STAY_ON_TOP);
}
static void on_grid_font(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("generic dmx: Grid Font... has no font dialog in LJ4EVR (the grid uses the form font)"); }   /* ponytail: TFontDialog not built; add with a font picker */

/* TMegaGridColorSchemeForm "Grid Color Scheme": the six swatches and Default All */
static void color_close(lj_widget *w, void *ud) { (void)w; (void)ud; if (g.color_win) { lj_win_hide(g.color_win); lj_window_destroy(g.color_win); g.color_win = NULL; } }
static void color_swatches(void)
{
    static const char *const pn[GC_COUNT] = { "pBack", "pChannel", "pBackSelected", "pOff", "pSnap", "pFade" };
    int i;
    for (i = 0; g.color_win && i < GC_COUNT; i++) { lj_widget *p = lj_window_find(g.color_win, pn[i]); if (p) { p->color = g.color[i]; lj_widget_invalidate(p); } }
}
static void colors_save(void) { lj_ctx *c = lj_ctx_get(); int i; for (i = 0; c->forms_ini && i < GC_COUNT; i++) lj_ini_seti(c->forms_ini, FORMS_SEC, gc_keys[i], (int)g.color[i]); }
static void on_color_panel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("generic dmx: no colour dialog in LJ4EVR; Default All restores the scheme"); }   /* ponytail: TColorDialog not built */
static void on_color_default(lj_widget *w, void *ud) { (void)w; (void)ud; memcpy(g.color, gc_default, sizeof g.color); colors_save(); color_swatches(); if (g.grid) lj_widget_invalidate(g.grid); }
static void on_grid_colors(lj_widget *w, void *ud)
{
    static const lj_binding b[] = { { "FormClose", color_close, NULL }, { "PanelClick", on_color_panel, NULL }, { "Button1Click", on_color_default, NULL }, { NULL, NULL, NULL } };
    (void)w; (void)ud;
    if (g.color_win || (g.color_win = lj_window_create(&lj_form_TMEGAGRIDCOLORSCHEMEFORM, b)) == NULL) return;
    color_swatches();
    lj_win_show(g.color_win, LJ_WIN_STAY_ON_TOP);
}

static void on_gdc_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_gdc_form_close(); }
static void on_resize(lj_widget *w, void *ud) { (void)w; (void)ud; cells_build(); }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_gdc_form_refresh(); }

static const lj_binding lj_gdc_bindings[] = {
    { "FormClose", on_gdc_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormKeyDown", noop, NULL } /* the keys reach eCommand (cmd_key) */,
    { "FormResize", on_resize, NULL }, { "FormShow", noop, NULL },
    { "pb1MouseDown", on_fader_down, NULL }, { "pb1MouseMove", on_fader_move, NULL }, { "pb1MouseUp", on_fader_up, NULL }, { "pb1Paint", on_fader_paint, NULL },
    { "pOffMouseDown", on_state_down, NULL }, { "pOffMouseUp", noop, NULL }, { "sbMouseDown", on_sb_down, NULL }, { "sbMouseUp", noop, NULL },
    { "SpeedButton3Click", on_clear_sel, NULL }, { "lv1Change", on_lv_change, NULL }, { "lv1Enter", noop, NULL }, { "lv1MouseDown", on_lv_down, NULL },
    { "Panel5Resize", noop, NULL }, { "eCommandChange", noop, NULL }, { "eCommandKeyDown", noop, NULL } /* cmd_key */,
    { "DG1DrawCell", on_draw_cell, NULL }, { "DG1MouseDown", on_grid_down, NULL }, { "DG1MouseMove", on_grid_move, NULL }, { "DG1MouseUp", on_grid_up, NULL },
    { "DeleteAllGroups1Click", on_delete_all, NULL }, { "ClearallGroups1Click", on_clear_all, NULL }, { "HideSelectedChannels1Click", on_hide, NULL },
    { "ShowAllChannels1Click", on_show_all, NULL }, { "PrefsClick", on_prefs, NULL }, { "GridSize1Click", on_grid_size, NULL }, { "GridFont1Click", on_grid_font, NULL },
    { "GridColors1Click", on_grid_colors, NULL }, { "Help1Click", noop, NULL }, { "Timer1Timer", on_timer, NULL }, { "FontDialog1Apply", noop, NULL },
    { "GridPopPopup", on_grid_pop, NULL }, { "PopClick", on_pop_click, NULL }, { "GroupPopPopup", on_group_pop, NULL },
    { "P_CreateGroupClick", on_create_group, NULL }, { "P_RenameGroupClick", on_rename_group, NULL }, { "P_DeleteGroupClick", on_delete_group, NULL },
    { "P_AddSelectedtoGroupClick", on_add_sel, NULL }, { "P_RemoveSelectedFromGroupClick", on_rem_sel, NULL }, { "P_SetGroupClick", on_set_group, NULL },
    { "P_ClearGroupClick", on_clear_group, NULL }, { "CaptureTimerTimer", noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_gdc_form_open(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[160];
    int i;
    if (g.win) return g.win;
    if (!g.groups_loaded && c->show && lib_path(LJ_GDC_GROUPS_FILE, path, sizeof path) == 0) {
        if (lj_gdc_groups_load(path, &c->show->gdc, err, sizeof err) != 0) lj_log("generic dmx: %s", err);
        g.groups_loaded = 1;
    }
    if ((g.win = lj_window_create(&lj_form_TGENERICMEGAFORM, lj_gdc_bindings)) == NULL) { lj_log("generic dmx: cannot create TGenericMegaForm"); return NULL; }
    g.grid = lj_window_find(g.win, "DG1"); g.fader = lj_window_find(g.win, "pb1"); g.cmd = lj_window_find(g.win, "eCommand");
    for (i = 0; i < GP_COUNT; i++) g.pref[i] = gp_rows[i].key && c->forms_ini ? lj_ini_geti(c->forms_ini, FORMS_SEC, gp_rows[i].key, gp_rows[i].dflt) : gp_rows[i].dflt;
    for (i = 0; i < GC_COUNT; i++) g.color[i] = c->forms_ini ? (uint32_t)lj_ini_geti(c->forms_ini, FORMS_SEC, gc_keys[i], (int)gc_default[i]) : gc_default[i];
    g.cw = c->forms_ini ? lj_ini_geti(c->forms_ini, FORMS_SEC, "CellWidth", 32) : 32;       /* DG1 DefaultColWidth 32 */
    g.rh = c->forms_ini ? lj_ini_geti(c->forms_ini, FORMS_SEC, "CellHight", 26) : 26;       /* two text rows; measured from genericcontrol.gif (reduced), PROPOSAL rig F35 */
    if (g.cw < 16 || g.cw > 64) g.cw = 32;
    if (g.rh < 16 || g.rh > 48) g.rh = 26;
    g.cols = 0; g.down = 0; g.rel = 0; g.relaid = 0;
    if (g.cmd) { g.cmd_vt = *g.cmd->vt; g.cmd_vt.key = cmd_key; g.cmd->vt = &g.cmd_vt; }
    if (g.grid) { g.grid_vt = *g.grid->vt; lj_w_grid_vt_mouse = g.grid_vt.mouse; g.grid_vt.mouse = grid_mouse; g.grid->vt = &g.grid_vt; }
    lv_hook_install(&g.lv_hook, lj_window_find(g.win, "lv1"), on_lv_change);
    groups_list();
    prefs_apply();
    lj_win_show(g.win, LJ_WIN_STAY_ON_TOP);                                                 /* FormStyle = fsStayOnTop */
    return g.win;
}
void lj_gdc_form_close(void)
{
    if (!g.win) return;
    size_close(NULL, NULL); color_close(NULL, NULL); create_close();
    lj_win_hide(g.win);
    lj_window_destroy(g.win);
    g.win = NULL; g.grid = g.fader = g.cmd = NULL;
}
lj_window *lj_gdc_form_get(void) { return g.win; }
void lj_gdc_form_refresh(void)
{
    if (!g.win) return;
    if (g.grid) lj_widget_invalidate(g.grid);
    if (g.fader) lj_widget_invalidate(g.fader);
}

/* ================= Sequence fades (TDefault2Form "Set Fade Status") ================= */

static const char *const sf_boxes[LJ_ATTR_COUNT] = { "cbIntensity", "cbMovement", "cbColor", "cbGobo", "cbBeam", "cbEffects", "cbLevel", "cbMovementmacro" };   /* enum lj_attr */
static lj_window *sf_win;

int lj_seq_fades_apply(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_seqfades f;
    uint8_t sel[LJ_FIXTURES];
    int i, n = -1;
    if (!sf_win || !c->show || !c->show->current_seq) return -1;
    memset(&f, 0, sizeof f);
    for (i = 0; i < LJ_ATTR_COUNT; i++) if (lj_w_checkbox_checked(lj_window_find(sf_win, sf_boxes[i]))) f.attr_mask = (uint8_t)(f.attr_mask | 1u << i);
    f.all_scenes = (uint8_t)lj_w_checkbox_checked(lj_window_find(sf_win, "cbAllScenes"));
    i = lj_w_radiogroup_index(lj_window_find(sf_win, "RG1"));
    f.sfx = (uint8_t)(i < 0 ? 0 : i);
    for (i = 0; i < LJ_FIXTURES; i++) sel[i] = (uint8_t)(lj_sel_is_selected(i) != 0);
    lj_lock();
    n = lj_seqfades_apply(&f, c->show, c->show->current_seq, c->current_scene, sel);
    lj_unlock();
    if (n > 0) scene_written();
    return n;
}
static void sf_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_seq_fades_apply(); lj_seq_fades_close(); }   /* bkOK */
static void sf_toggle(lj_widget *w, void *ud)                                         /* sbToggle "Select All": all on, or all off when all are on (PROPOSAL rig 04 s1.4) */
{
    int i, all = 1;
    (void)w; (void)ud;
    for (i = 0; i < LJ_ATTR_COUNT; i++) all &= lj_w_checkbox_checked(lj_window_find(sf_win, sf_boxes[i])) != 0;
    for (i = 0; i < LJ_ATTR_COUNT; i++) lj_w_checkbox_set_checked(lj_window_find(sf_win, sf_boxes[i]), !all);
}
static void sf_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_seq_fades_close(); }
static const lj_binding lj_sf_bindings[] = {
    { "FormActivate", noop, NULL }, { "FormClose", sf_close, NULL }, { "bOkClick", sf_ok, NULL }, { "sbToggleClick", sf_toggle, NULL },
    { "cbIntensityClick", noop, NULL }, { "help1Click", noop, NULL }, { NULL, NULL, NULL } };

lj_window *lj_seq_fades_open(void)
{
    if (sf_win) return sf_win;
    if ((sf_win = lj_window_create(&lj_form_TDEFAULT2FORM, lj_sf_bindings)) == NULL) { lj_log("sequence fades: cannot create TDefault2Form"); return NULL; }
    lj_win_show(sf_win, LJ_WIN_STAY_ON_TOP);
    return sf_win;
}
void lj_seq_fades_close(void) { if (!sf_win) return; lj_win_hide(sf_win); lj_window_destroy(sf_win); sf_win = NULL; }
lj_window *lj_seq_fades_get(void) { return sf_win; }
void lj_seq_fades_refresh(void) { }

/* ================= Follow spot settings (TFollowSpotForm, TFollowListForm) ================= */

static struct { lj_window *win, *list; int row, modified, loaded; char cell[LJ_FOLLOWSPOT][4][80]; char lines[LJ_FOLLOWSPOT][48]; const char *items[LJ_FOLLOWSPOT]; int item_set[LJ_FOLLOWSPOT], nitems; lv_hook lv_hook; } fs;

static void fs_load(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[160];
    if (fs.loaded || !c->show || lib_path(LJ_FOLLOWSPOT_FILE, path, sizeof path) != 0) return;
    if (lj_followspot_load(path, c->show->followspot, err, sizeof err) != 0) lj_log("follow spot: %s", err);
    fs.loaded = 1;
}
static int fs_save(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!c->show || lib_path(LJ_FOLLOWSPOT_FILE, path, sizeof path) != 0 || lj_followspot_save(path, c->show->followspot) != 0) { lj_log("follow spot: cannot save " LJ_FOLLOWSPOT_FILE); return -1; }
    fs.modified = 0;
    return 0;
}
static void fs_fill(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *g1 = fs.win ? lj_window_find(fs.win, "sg1") : NULL;
    char t[64];
    int r;
    if (!g1 || !c->show) return;
    for (r = 0; r < LJ_FOLLOWSPOT; r++) {
        const lj_followspot *f = &c->show->followspot[r];
        const lj_static_slot *sl = NULL;
        if (f->static_page && f->static_page <= c->show->npages && c->show->pages && f->static_slot) sl = &c->show->pages[f->static_page - 1].slot[f->static_slot - 1];
        snprintf(fs.cell[r][0], sizeof fs.cell[r][0], "%d", r + 1);
        lj_str_copy(fs.cell[r][1], sizeof fs.cell[r][1], f->used ? "On" : "Off");
        lj_str_copy(fs.cell[r][2], sizeof fs.cell[r][2], f->name);
        if (f->static_page) snprintf(fs.cell[r][3], sizeof fs.cell[r][3], "%d: %s %s", f->static_slot, sl ? sl->name : "", f->static_auto_clear ? "(autoclr)" : "");   /* exe 0x1A34D4 "%d: %s %s", 0x1A3338 "(autoclr)"; the number = the slot (PROPOSAL rig 04 s13) */
        else lj_str_copy(fs.cell[r][3], sizeof fs.cell[r][3], "-");                                                         /* editfollowspot.gif */
        lj_w_grid_set_cell(g1, 0, r, fs.cell[r][0]); lj_w_grid_set_cell(g1, 1, r, fs.cell[r][1]);
        lj_w_grid_set_cell(g1, 2, r, fs.cell[r][2]); lj_w_grid_set_cell(g1, 3, r, fs.cell[r][3]);
    }
    snprintf(t, sizeof t, "&Apply Current Scene to %d", fs.row + 1); set_caption(fs.win, "bApplyScene", t);   /* exe 0x1A2D94 */
    snprintf(t, sizeof t, "&Clear Settings in %d", fs.row + 1); set_caption(fs.win, "bClear", t);           /* exe 0x1A2DB8 */
    lj_widget_invalidate(g1);
}
static void fs_cols(void)                     /* four equal columns: number, On/Off, name, static */
{
    lj_widget *g1 = fs.win ? lj_window_find(fs.win, "sg1") : NULL;
    if (g1) ((lj_w_grid *)g1)->def_cw = (g1->w - 4) / 4;   /* ponytail: editfollowspot.gif has 20 / 30 / 150 / rest; lj_w_grid has one column width (per-column widths are an src/ui field) */
}

int lj_followspot_form_apply_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc;
    uint8_t sel[LJ_FIXTURES];
    int i, nsel = 0, nrec = 0, rc = -1;
    if (!c->show) return -1;
    for (i = 0; i < LJ_FIXTURES; i++) nsel += (sel[i] = (uint8_t)(lj_sel_is_selected(i) != 0));
    lj_lock();
    if ((sc = cur_scene()) != NULL) rc = lj_followspot_record(&c->show->followspot[fs.row], c->show, sc, sel);
    lj_unlock();
    if (rc != 0) { lj_win_message("Edit FollowSpot Settings", "No Followspot Fixtures Selected"); return -1; }   /* exe 0x1A3050 */
    for (i = 0; i < LJ_FIXTURES; i++) nrec += c->show->followspot[fs.row].selected[i];
    if (nrec < nsel) lj_win_message("Edit FollowSpot Settings", "Non Followspot Fixtures Ignored");                /* exe 0x1A3078 */
    fs.modified = 1;
    fs_fill();
    return 0;
}
static void fs_apply(lj_widget *w, void *ud) { (void)w; (void)ud; lj_followspot_form_apply_scene(); }
static void fs_clear(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)w; (void)ud;
    if (!c->show) return;
    memset(&c->show->followspot[fs.row], 0, sizeof c->show->followspot[fs.row]);
    fs.modified = 1; fs_fill();
}
static void fs_clear_all_result(int yes, void *ud) { lj_ctx *c = lj_ctx_get(); (void)ud; if (yes && c->show) { memset(c->show->followspot, 0, sizeof c->show->followspot); fs.modified = 1; fs_fill(); } }
static void fs_clear_all(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Edit FollowSpot Settings", "Clear all followspot settings, are you sure ?", "", fs_clear_all_result, NULL); }   /* exe 0x1A3300 */
static void fs_save_click(lj_widget *w, void *ud) { (void)w; (void)ud; fs_save(); }
static void fs_select(lj_widget *w, void *ud)
{
    lj_w_grid *gr = (lj_w_grid *)w;
    (void)ud;
    if (gr->row >= 0 && gr->row < LJ_FOLLOWSPOT) fs.row = gr->row;
    set_enabled(fs.win, "bApplyScene", 1); set_enabled(fs.win, "bClear", 1);
    fs_fill();
}
static void fs_static_clear(lj_widget *w, void *ud) { lj_ctx *c = lj_ctx_get(); (void)w; (void)ud; if (c->show) { c->show->followspot[fs.row].static_page = c->show->followspot[fs.row].static_slot = 0; c->show->followspot[fs.row].static_auto_clear = 0; fs.modified = 1; fs_fill(); } }
static void fs_static_autoclr(lj_widget *w, void *ud) { lj_ctx *c = lj_ctx_get(); (void)w; (void)ud; if (c->show && c->show->followspot[fs.row].static_page) { c->show->followspot[fs.row].static_auto_clear ^= 1; fs.modified = 1; fs_fill(); } }
static void fs_pop(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    lj_menu *m = lj_window_menus(fs.win);
    int has = c->show && c->show->followspot[fs.row].static_page;
    (void)w; (void)ud;
    if (!m) return;
    lj_menu_set_enabled(m, "M_RemoveStatic", has); lj_menu_set_enabled(m, "M_AutoClearStatic", has);
    lj_menu_set_checked(m, "M_AutoClearStatic", has && c->show->followspot[fs.row].static_auto_clear);
    lj_menu_set_enabled(m, "M_AssignStatic", 0);   /* ponytail: LJ2 fills "Set Static..." with the statics at run time; the submenu needs a runtime-items seam in lj_menu */
}
static void fs_close_result(int yes, void *ud) { (void)ud; if (yes) fs_save(); fs.modified = 0; lj_followspot_form_close(); }
static void fs_form_close(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (fs.modified) { lj_win_confirm("Edit FollowSpot Settings", "Followspot settings changed but not saved - save now?", "", fs_close_result, NULL); return; }   /* exe 0x1A2B6C */
    lj_followspot_form_close();
}
static void fs_resize(lj_widget *w, void *ud) { (void)w; (void)ud; fs_cols(); }
static const lj_binding lj_fs_bindings[] = {
    { "FormClose", fs_form_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormResize", fs_resize, NULL },
    { "bSaveClick", fs_save_click, NULL }, { "bApplySceneClick", fs_apply, NULL }, { "bClearClick", fs_clear, NULL },
    { "sg1GetEditMask", noop, NULL }, { "sg1MouseDown", noop, NULL }, { "sg1SelectCell", fs_select, NULL },
    { "ClearAllFollowspotSettings1Click", fs_clear_all, NULL }, { "Help1Click", noop, NULL }, { "popPopup", fs_pop, NULL },
    { "M_ApplyCurrentSceneClick", fs_apply, NULL }, { "ClearEntry1Click", fs_clear, NULL }, { "M_RemoveStaticClick", fs_static_clear, NULL },
    { "M_AutoClearStaticClick", fs_static_autoclr, NULL }, { NULL, NULL, NULL } };

lj_window *lj_followspot_form_open(void)
{
    if (fs.win) return fs.win;
    fs_load();
    if ((fs.win = lj_window_create(&lj_form_TFOLLOWSPOTFORM, lj_fs_bindings)) == NULL) { lj_log("follow spot: cannot create TFollowSpotForm"); return NULL; }
    fs.row = 0; fs.modified = 0;
    fs_cols();
    fs_fill();
    lj_win_show(fs.win, LJ_WIN_STAY_ON_TOP);
    return fs.win;
}
void lj_followspot_form_close(void) { if (!fs.win) return; lj_win_hide(fs.win); lj_window_destroy(fs.win); fs.win = NULL; lj_followspot_list_refresh(); }
lj_window *lj_followspot_form_get(void) { return fs.win; }
void lj_followspot_form_refresh(void) { }

/* TFollowListForm "Select Followspot Setting": the used settings by name; OK / double-click activates, Clear releases */
static int fl_setting(void) { lj_widget *lv = fs.list ? lj_window_find(fs.list, "lv1") : NULL; int i = lv ? ((lj_w_listview *)lv)->index : -1; return i >= 0 && i < fs.nitems ? fs.item_set[i] + 1 : 0; }
static void fl_ok(lj_widget *w, void *ud) { int n = fl_setting(); (void)w; (void)ud; if (n) { lj_desktop_run_action(LJ_ACT_FOLLOWSPOT_APPLY, n); lj_followspot_list_close(); } }
static void fl_clear(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_FOLLOWSPOT_APPLY, 0); }
static void fl_change(lj_widget *w, void *ud) { (void)w; (void)ud; set_enabled(fs.list, "bOk", fl_setting() != 0); }
static void fl_edit(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_FOLLOWSPOT_FORM); }
static void fl_presel(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    int on;
    (void)w; (void)ud;
    if (!c->main_ini) return;
    on = !lj_ini_geti(c->main_ini, "MISC", "FollowModePreSelected", 0);   /* key verbatim (exe 0x3237A8); section PROPOSAL rig 04 s13 */
    lj_ini_seti(c->main_ini, "MISC", "FollowModePreSelected", on);
    lj_menu_set_checked(lj_window_menus(fs.list), "M_Presel", on);
}
static void fl_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_followspot_list_close(); }
static const lj_binding lj_fl_bindings[] = {
    { "FormClose", fl_close, NULL }, { "FormCreate", noop, NULL }, { "bOkClick", fl_ok, NULL }, { "bClearClick", fl_clear, NULL },
    { "lv1Change", fl_change, NULL }, { "lv1DblClick", fl_ok, NULL }, { "FollowspotSetttings1Click", fl_edit, NULL },
    { "Preferences1Click", noop, NULL }, { "M_PreselClick", fl_presel, NULL }, { "Help1Click", noop, NULL }, { NULL, NULL, NULL } };

lj_window *lj_followspot_list_open(void)
{
    lj_ctx *c = lj_ctx_get();
    if (fs.list) return fs.list;
    fs_load();
    if ((fs.list = lj_window_create(&lj_form_TFOLLOWLISTFORM, lj_fl_bindings)) == NULL) { lj_log("follow spot: cannot create TFollowListForm"); return NULL; }
    lj_menu_set_checked(lj_window_menus(fs.list), "M_Presel", c->main_ini && lj_ini_geti(c->main_ini, "MISC", "FollowModePreSelected", 0));
    lv_hook_install(&fs.lv_hook, lj_window_find(fs.list, "lv1"), fl_change);
    lj_followspot_list_refresh();
    lj_win_show(fs.list, LJ_WIN_STAY_ON_TOP);
    return fs.list;
}
void lj_followspot_list_close(void) { if (!fs.list) return; lj_win_hide(fs.list); lj_window_destroy(fs.list); fs.list = NULL; }
lj_window *lj_followspot_list_get(void) { return fs.list; }
void lj_followspot_list_refresh(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *lv = fs.list ? lj_window_find(fs.list, "lv1") : NULL;
    int r;
    if (!lv || !c->show) return;
    fs.nitems = 0;
    for (r = 0; r < LJ_FOLLOWSPOT; r++) {
        const lj_followspot *f = &c->show->followspot[r];
        if (!f->used) continue;
        if (f->name[0]) lj_str_copy(fs.lines[fs.nitems], sizeof fs.lines[fs.nitems], f->name); else snprintf(fs.lines[fs.nitems], sizeof fs.lines[fs.nitems], "Setting# %d", r + 1);   /* exe 0x1A32B0 "Setting# " */
        fs.items[fs.nitems] = fs.lines[fs.nitems]; fs.item_set[fs.nitems++] = r;
    }
    lj_w_listview_set_items(lv, fs.items, fs.nitems);
    set_enabled(fs.list, "bOk", fl_setting() != 0);
}

/* ================= hotkeys (01 s7) ================= */
static void fn_generic(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)fn; (void)prog; (void)ud; if (edge) lj_win_toggle(LJ_WIN_GDC_FORM); }
static void fn_follow(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)prog; (void)ud; if (edge) lj_desktop_run_action(LJ_ACT_FOLLOWSPOT_APPLY, fn == LJ_FN_CLEAR_FOLLOWSPOT ? 0 : (int)(fn - LJ_FN_FOLLOW_SPOT_1) + 1); }
void lj_gdc_form_bind_fns(void)
{
    int i;
    lj_fn_bind(LJ_FN_GENERIC_CONTROLS, fn_generic, NULL);
    for (i = 0; i < 10; i++) lj_fn_bind((enum lj_fn)(LJ_FN_FOLLOW_SPOT_1 + i), fn_follow, NULL);
    lj_fn_bind(LJ_FN_CLEAR_FOLLOWSPOT, fn_follow, NULL);
}
