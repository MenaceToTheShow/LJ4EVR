/* lj_cue_pages.c - cue pages: List of Cues page side, TGetPageInfoForm, TReorderCuePagesForm (lj_cue_pages.h). */
#include "windows/lj_cue_pages.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "model/lj_library.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#define LJ_CP_FILE        "Cues\\CuePageInfo.Dat"
#define LJ_CP_FIRST_PAGE  "Page 1"      /* TC1 Tabs.Strings and exe 0x161014: the page a library without CuePageInfo.Dat shows */
#define LJ_CP_NEW_NAME    "New Page"    /* exe 0x1628C0 beside "Insert New Cue Page": the name field's default (PROPOSAL rig 13 s5) */
#define LJ_DND_PAGE_ROW   0x7001        /* drag payload kind private to the Re-order dialog */

static lj_cuepages lj_cp_own;           /* used when the app did not set ctx->cue_pages */
static struct {
    int loaded, locked;
    uint32_t clip; int has_clip;
    const char *tabs[LJ_CUE_PAGES]; char rows[LJ_CUE_PAGES][48]; const char *rowp[LJ_CUE_PAGES];
} cp;

static lj_ctx *C(void) { return lj_ctx_get(); }
static void acs_seti(const char *key, int v) { lj_ctx *c = C(); if (c->forms_ini) lj_ini_seti(c->forms_ini, "ACUESELECT", key, v); }

lj_cuepages *lj_cue_pages_model(void)
{
    lj_ctx *c = C();
    char path[LJ_PATH_CAP], err[128];
    if (!c->cue_pages) {                                     /* the app owns the record when it loaded one; else ours, read once */
        lj_cuepages_init(&lj_cp_own);
        if (c->lib && lj_library_path(c->lib, LJ_CP_FILE, path, sizeof path) == 0 && lj_cuepages_load(path, &lj_cp_own, err, sizeof err) != 0) {
            lj_log("cue pages: %s: %s", path, err);
            lj_win_message("Error", "Error Reading Cuepage data file - Cuepage info is skipped");   /* exe 0x18A328 */
            lj_cuepages_init(&lj_cp_own);
        }
        c->cue_pages = &lj_cp_own;
    }
    if (c->cue_pages->n == 0 && lj_cuepages_add(c->cue_pages, LJ_CP_FIRST_PAGE) == 0) c->cue_pages->current = 0;
    if (!cp.loaded) { cp.loaded = 1; c->cue_pages->current = lj_ini_geti(lj_ctx_get()->forms_ini, "ACUESELECT", "Page", 0); }
    if (c->cue_pages->current < 0 || c->cue_pages->current >= c->cue_pages->n) c->cue_pages->current = 0;
    return c->cue_pages;
}

int lj_cue_pages_store(void)
{
    lj_ctx *c = C();
    char path[LJ_PATH_CAP];
    lj_cuepages *p = lj_cue_pages_model();
    acs_seti("Page", p->current);
    if (!c->lib) return 0;                                   /* headless without a library: memory only */
    if (lj_library_path(c->lib, LJ_CP_FILE, path, sizeof path) != 0 || lj_cuepages_save(path, p) != 0) {
        lj_win_message("Error", "Error Saving Cue Page File");   /* exe 0x164114 */
        return -1;
    }
    return 0;
}

/* ---- the List of Cues page side ---- */
static lj_widget *lc_find(const char *name) { lj_window *w = lj_list_cue_get(); return w ? lj_window_find(w, name) : NULL; }
static int lv_index(void) { lj_widget *lv = lc_find("lvPages"); return lv ? lj_w_listview_index(lv) : -1; }

void lj_cue_pages_fill(lj_window *lc)
{
    lj_cuepages *p = lj_cue_pages_model();
    lj_ctx *c = C();
    const lj_cue_page *pg = &p->page[p->current];
    lj_widget *w;
    int i;
    if (!lc) return;
    for (i = 0; i < p->n; i++) cp.tabs[i] = p->page[i].name;
    if ((w = lj_window_find(lc, "TC1")) != NULL) {
        lj_w_tabs *t = (lj_w_tabs *)w;
        t->tabs = cp.tabs; t->ntabs = p->n; t->active = p->current;
        lj_widget_layout(w);
        lj_widget_invalidate(w);
    }
    for (i = 0; i < pg->n; i++) {
        const lj_cue *rec = c->show ? (const lj_cue *)lj_table_get(&c->show->cues, pg->cue[i]) : NULL;
        snprintf(cp.rows[i], sizeof cp.rows[i], "%s%04u\t%s", rec && rec->transparent ? "(T)" : "", (unsigned)pg->cue[i], rec ? rec->name : "");   /* columns Cue# | Cue Name; (T) as the global list */
        cp.rowp[i] = cp.rows[i];
    }
    if ((w = lj_window_find(lc, "lvPages")) != NULL) {
        lj_w_listview_set_items(w, cp.rowp, pg->n);
        ((lj_w_listview *)w)->index = p->cursor < pg->n ? p->cursor : -1;
    }
}
static void refill(void) { lj_window *lc = lj_list_cue_get(); if (lc) lj_cue_pages_fill(lc); }
static void changed(void) { lj_cue_pages_store(); refill(); }

static void load(uint32_t cue)
{
    const lj_cue_form_host *h = lj_cue_form_get_host();
    if (!cue) return;
    if (!h || !h->load_cue) { lj_log("cue pages: load cue %04u: no loader (headless)", (unsigned)cue); return; }
    if (h->load_cue(h->ud, cue, 0) != 0) lj_log("cue pages: load cue %04u failed", (unsigned)cue);
}

int lj_cue_pages_step_page(int delta)
{
    lj_cuepages *p = lj_cue_pages_model();
    int r = lj_cuepages_step(p, delta, lj_ini_geti(lj_ctx_get()->forms_ini, "ACUESELECT", "CircularPageSelect", 0));
    if (r >= 0) { p->cursor = -1; acs_seti("Page", p->current); refill(); }
    return r;
}
uint32_t lj_cue_pages_step_cue(int delta, int latch)
{
    lj_cuepages *p = lj_cue_pages_model();
    uint32_t cue;
    int sel = lv_index();
    if (sel >= 0) p->cursor = sel;                         /* the row the operator clicked */
    cue = lj_cuepages_step_cue(p, delta);
    if (!cue && lj_ini_geti(lj_ctx_get()->forms_ini, "ACUESELECT", "NPChangePage", 0) && lj_cuepages_step(p, delta, lj_ini_geti(lj_ctx_get()->forms_ini, "ACUESELECT", "CircularPageSelect", 0)) >= 0) {   /* "Next/Prev Cue changes page" */
        p->cursor = delta > 0 ? -1 : p->page[p->current].n;
        cue = lj_cuepages_step_cue(p, delta);
    }
    refill();
    if (cue && latch) load(cue);
    return cue;
}

static void confirm_then(const char *what, void (*fn)(int yes, void *ud))
{
    lj_win_confirm(NULL, what, "Are you sure?", fn, NULL);   /* PROPOSAL rig 13 s5: the exe strings "Clear Current Cue Page" ... + its "Are you sure?" (0x1B97E0); the box layout is TCONFIRMFORM */
}
static void do_clear_page(int yes, void *ud)  { lj_cuepages *p = lj_cue_pages_model(); (void)ud; if (yes && lj_cuepages_clear(p, p->current) == 0) { p->cursor = -1; changed(); } }
static void do_delete_page(int yes, void *ud) { lj_cuepages *p = lj_cue_pages_model(); (void)ud; if (yes && lj_cuepages_delete(p, p->current) == 0) { lj_cue_pages_model(); p->cursor = -1; changed(); } }
static void do_clear_all(int yes, void *ud)
{
    lj_cuepages *p = lj_cue_pages_model();
    int i;
    (void)ud;
    if (!yes) return;
    for (i = 0; i < p->n; i++) lj_cuepages_clear(p, i);
    p->cursor = -1;
    changed();
}
static void do_delete_all(int yes, void *ud)
{
    lj_cuepages *p = lj_cue_pages_model();
    (void)ud;
    if (!yes) return;
    while (p->n > 0) lj_cuepages_delete(p, p->n - 1);
    p->current = 0; p->cursor = -1;
    lj_cue_pages_model();                                    /* back to the one "Page 1" */
    changed();
}

int lj_cue_pages_add_global(uint32_t cue, int insert)
{
    lj_cuepages *p = lj_cue_pages_model();
    int at = insert ? (lv_index() >= 0 ? lv_index() : 0) : -1;
    if (cp.locked || !cue) return -1;
    if (lj_cuepages_add_cue(p, p->current, cue, at) < 0) return -1;
    changed();
    return 0;
}

int lj_cue_pages_command(const char *item)
{
    lj_cuepages *p = lj_cue_pages_model();
    lj_ctx *c = C();
    uint32_t cur = c->show && c->show->current_cue ? c->show->current_cue->number : 0;
    int sel = lv_index();
    if (!item) return -1;
    if (!strcmp(item, "M_AddnewPage") || !strcmp(item, "PopAddNewPage")) lj_cue_page_name_open_mode(LJ_PAGE_NAME_ADD);
    else if (!strcmp(item, "PopInsertNewPage")) lj_cue_page_name_open_mode(LJ_PAGE_NAME_INSERT);
    else if (!strcmp(item, "PopEditPageName")) lj_cue_page_name_open_mode(LJ_PAGE_NAME_EDIT);
    else if (!strcmp(item, "M_ReorderPages")) lj_win_open(LJ_WIN_CUE_PAGES_REORDER);
    else if (!strcmp(item, "M_ClearAllPages")) lj_win_confirm(NULL, "Clear All Pages ?", NULL, do_clear_all, NULL);   /* exe 0x1E1C68 */
    else if (!strcmp(item, "M_DeleteAllPages")) confirm_then("Delete all Cue Pages", do_delete_all);
    else if (!strcmp(item, "PopClearPage")) confirm_then("Clear Current Cue Page", do_clear_page);
    else if (!strcmp(item, "PopDeletePage")) confirm_then("Delete Current Cue Page", do_delete_page);
    else if (!strcmp(item, "PopLoadPageCues")) { if (sel >= 0 && sel < p->page[p->current].n) load(p->page[p->current].cue[sel]); }
    else if (!strcmp(item, "PopLockList")) {
        lj_window *lc = lj_list_cue_get();
        cp.locked = !cp.locked;
        if (lc) lj_menu_set_checked(lj_window_menus(lc), "PopLockList", cp.locked);
    }
    else if (cp.locked) return 0;                            /* Lock Page Lists: the editing items below do nothing */
    else if (!strcmp(item, "PopPageCopy") || !strcmp(item, "PopPageCut")) {
        if (sel < 0 || sel >= p->page[p->current].n) return 0;
        cp.clip = p->page[p->current].cue[sel]; cp.has_clip = 1;
        if (!strcmp(item, "PopPageCut") && lj_cuepages_remove_cue(p, p->current, sel) == 0) changed();
    }
    else if (!strcmp(item, "PopPagePaste")) { if (cp.has_clip && lj_cuepages_add_cue(p, p->current, cp.clip, sel) >= 0) changed(); }
    else if (!strcmp(item, "PopClearSelectedCues")) { if (sel >= 0 && lj_cuepages_remove_cue(p, p->current, sel) == 0) { p->cursor = -1; changed(); } }
    else if (!strcmp(item, "PopAddCurrentCue")) { if (cur && lj_cuepages_add_cue(p, p->current, cur, -1) >= 0) changed(); }
    else if (!strcmp(item, "PopInsertCurrentCue")) { if (cur && lj_cuepages_add_cue(p, p->current, cur, sel >= 0 ? sel : 0) >= 0) changed(); }
    else return -1;
    return 0;
}

void lj_cue_pages_on_tab(lj_widget *w, void *ud)
{
    lj_cuepages *p = lj_cue_pages_model();
    int i = ((lj_w_tabs *)w)->active;
    (void)ud;
    if (i < 0 || i >= p->n || i == p->current) return;
    p->current = i; p->cursor = -1;
    acs_seti("Page", i);
    refill();
}
void lj_cue_pages_on_menu(lj_widget *w, void *ud)
{
    lj_window *lc = lj_list_cue_get();
    const lj_menu_item_desc *it = lc ? lj_window_menu_item(lc) : NULL;
    (void)w; (void)ud;
    if (it && lj_cue_pages_command(it->name) != 0) lj_log("cue pages: %s not handled", it->name ? it->name : "?");
}
void lj_cue_pages_on_button(lj_widget *w, void *ud)
{
    lj_window *lc = lj_list_cue_get();
    const lj_mouse_ev *m = lc ? lj_window_last_event(lc) : NULL;
    int latch = !(m && m->button == 2);                      /* right-click selects without latching [03 s5.8] */
    const char *n = w->name ? w->name : "";
    (void)ud;
    if (!strcmp(n, "sbPrevPage")) lj_cue_pages_step_page(-1);
    else if (!strcmp(n, "sbNextPage")) lj_cue_pages_step_page(1);
    else if (!strcmp(n, "sbPrev")) lj_cue_pages_step_cue(-1, latch);
    else if (!strcmp(n, "sbNext")) lj_cue_pages_step_cue(1, latch);
    else if (!strcmp(n, "sbPageGo")) lj_cue_pages_command("PopLoadPageCues");
}
void lj_cue_pages_on_dblclick(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cue_pages_command("PopLoadPageCues"); }
void lj_cue_pages_on_drop(lj_widget *w, void *ud)
{
    lj_window *lc = lj_list_cue_get();
    lj_cuepages *p = lj_cue_pages_model();
    int kind, number, x, y, row;
    (void)ud;
    if (!lc || cp.locked || lj_dnd_info(lc, &kind, &number, &x, &y) != 0 || kind != LJ_REC_CUE || number <= 0) return;
    row = lj_w_listview_row_at(w, y);
    if (lj_cuepages_add_cue(p, p->current, (uint32_t)number, row) >= 0) changed();   /* a drop past the rows appends (row -1) */
}

/* List of Cues Preferences: the exe's [ACUESELECT] keys where the literal pool has them (13 s6), PROPOSAL names for the rest */
static const struct { const char *item, *key; int dflt; } lj_acs_prefs[] = {
    { "M_ViewGlobal", "ViewGlobal", 1 } /* PROPOSAL key */, { "M_ViewPages", "ViewPages", 1 }, { "M_GlobalCueEntry", "GlobalCueEntry", 1 } /* PROPOSAL key */,
    { "M_TrackActiveCue", "TrackActiveCue", 0 } /* PROPOSAL key */, { "M_MultiLinePagetabs", "MultiLinePages", 0 } /* PROPOSAL key */,
    { "M_CircularPageSelection", "CircularPageSelect", 0 }, { "M_AutoPageChange", "AutoPageChange", 0 },
    { "M_LoadCueonEnter", "LoadOnEnter", 1 } /* PROPOSAL key */, { "M_BrowseCues", "Browse", 0 } /* PROPOSAL key */,
    { "M_EnableMultipleCues", "MultiCue", 0 } /* PROPOSAL key */, { "M_NextPrevChangePage", "NPChangePage", 0 }, { "M_ColorTrans", "ColorTrans", 1 } /* PROPOSAL key */ };
static void sync_prefs(lj_window *lc)
{
    lj_menu *m = lc ? lj_window_menus(lc) : NULL;
    int i;
    if (!m) return;
    for (i = 0; i < LJ_COUNT_OF(lj_acs_prefs); i++) lj_menu_set_checked(m, lj_acs_prefs[i].item, lj_ini_geti(lj_ctx_get()->forms_ini, "ACUESELECT", lj_acs_prefs[i].key, lj_acs_prefs[i].dflt));
    lj_menu_set_checked(m, "PopLockList", cp.locked);
}
void lj_cue_pages_on_prefs(lj_widget *w, void *ud)
{
    lj_window *lc = lj_list_cue_get();
    const lj_menu_item_desc *it = lc ? lj_window_menu_item(lc) : NULL;
    int i;
    (void)w; (void)ud;
    if (it && it->name) for (i = 0; i < LJ_COUNT_OF(lj_acs_prefs); i++)
        if (!strcmp(it->name, lj_acs_prefs[i].item)) { acs_seti(lj_acs_prefs[i].key, !lj_ini_geti(lj_ctx_get()->forms_ini, "ACUESELECT", lj_acs_prefs[i].key, lj_acs_prefs[i].dflt)); break; }
    sync_prefs(lc);                                          /* the Preferences1 parent click lands here too: it just re-syncs the checks */
}

/* ---- TGetPageInfoForm ---- */
static struct { lj_window *win; enum lj_page_name_mode mode; } pn;
lj_window *lj_cue_page_name_get(void) { return pn.win; }
void lj_cue_page_name_close(void) { if (pn.win) { lj_win_hide(pn.win); lj_window_destroy(pn.win); pn.win = NULL; } }
static void pn_ok(lj_widget *w, int ev, void *ud)
{
    lj_cuepages *p = lj_cue_pages_model();
    const char *name;
    int r = -1;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK || !pn.win) return;
    name = lj_w_edit_text(lj_window_find(pn.win, "Edit1"));
    if (!name) name = "";
    if (pn.mode == LJ_PAGE_NAME_ADD) { r = lj_cuepages_add(p, name); if (r >= 0) p->current = r; }
    else if (pn.mode == LJ_PAGE_NAME_INSERT) { r = lj_cuepages_insert(p, p->current, name); if (r >= 0) p->current = r; }
    else r = lj_cuepages_rename(p, p->current, name);
    lj_cue_page_name_close();
    if (r < 0) { lj_log("cue pages: page %d not changed (255 pages max)", p->current); return; }
    p->cursor = -1;
    changed();
}
static void pn_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lj_cue_page_name_close(); }
lj_window *lj_cue_page_name_open_mode(enum lj_page_name_mode mode)
{
    static const lj_binding b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    static const char *const caps[3] = { "Add New Cue Page", "Insert New Cue Page", "Edit Page Name" };   /* DFM caption; exe 0x1628A4, 0x161494 */
    lj_cuepages *p = lj_cue_pages_model();
    lj_widget *w;
    lj_cue_page_name_close();
    pn.win = lj_window_create(&lj_form_TGETPAGEINFOFORM, b);
    if (!pn.win) { lj_log("cue pages: TGetPageInfoForm failed to build"); return NULL; }
    pn.mode = mode;
    lj_window_set_caption(pn.win, caps[mode]);
    if ((w = lj_window_find(pn.win, "Edit1")) != NULL) { lj_w_edit_set_text(w, mode == LJ_PAGE_NAME_EDIT ? p->page[p->current].name : LJ_CP_NEW_NAME); lj_w_edit_select(w, 0, -1); lj_widget_set_focus(w); }
    if ((w = lj_window_find(pn.win, "BitBtn1")) != NULL) { w->on_event = pn_ok; w->ud = NULL; }       /* bkOK / bkCancel carry no OnClick in the DFM */
    if ((w = lj_window_find(pn.win, "BitBtn2")) != NULL) { w->on_event = pn_cancel; w->ud = NULL; }
    lj_win_show(pn.win, 0);
    return pn.win;
}
lj_window *lj_cue_page_name_open(void) { return lj_cue_page_name_open_mode(LJ_PAGE_NAME_ADD); }

/* ---- TReorderCuePagesForm ---- */
static struct { lj_window *win; lj_cuepages work; const char *rows[LJ_CUE_PAGES]; void (*lv_event)(lj_widget *, int, void *); void *lv_ud; } ro;
lj_window *lj_cue_pages_reorder_get(void) { return ro.win; }
void lj_cue_pages_reorder_close(void) { if (ro.win) { lj_win_hide(ro.win); lj_window_destroy(ro.win); ro.win = NULL; } }
static void ro_fill(void)
{
    lj_widget *lv = ro.win ? lj_window_find(ro.win, "lvPages") : NULL;
    int i;
    for (i = 0; i < ro.work.n; i++) ro.rows[i] = ro.work.page[i].name;
    if (lv) lj_w_listview_set_items(lv, ro.rows, ro.work.n);
}
int lj_cue_pages_reorder_move(int from, int to)
{
    if (!ro.win || lj_cuepages_move(&ro.work, from, to) != 0) return -1;
    ro_fill();
    if (ro.win) ((lj_w_listview *)lj_window_find(ro.win, "lvPages"))->index = to;
    return 0;
}
static void ro_lv_event(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ro.lv_event) ro.lv_event(w, ev, ro.lv_ud);
    if (ev == LJ_WEV_CLICK && lj_w_listview_index(w) >= 0) lj_dnd_begin(w, LJ_DND_PAGE_ROW, lj_w_listview_index(w));   /* DragMode = dmAutomatic */
}
static void ro_drop(lj_widget *w, void *ud)
{
    int kind, number, x, y, row;
    (void)ud;
    if (lj_dnd_info(ro.win, &kind, &number, &x, &y) != 0 || kind != LJ_DND_PAGE_ROW) return;
    row = lj_w_listview_row_at(w, y);
    lj_cue_pages_reorder_move(number, row < 0 ? ro.work.n - 1 : row);
}
static void ro_ok(lj_widget *w, int ev, void *ud)
{
    lj_cuepages *p = lj_cue_pages_model();
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    ro.work.current = p->current < ro.work.n ? p->current : 0; ro.work.cursor = -1;
    *p = ro.work;
    lj_cue_pages_reorder_close();
    changed();
}
static void ro_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lj_cue_pages_reorder_close(); }
static void ro_show(lj_widget *w, void *ud) { (void)w; (void)ud; ro_fill(); }
lj_window *lj_cue_pages_reorder_open(void)
{
    static const lj_binding b[] = { { "lvPagesDragDrop", ro_drop, NULL }, { "lvPagesDragOver", lj_handler_noop, NULL }, { "Help1Click", lj_handler_noop, NULL },
                                    { "FormActivate", ro_show, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    lj_widget *w;
    if (ro.win) return ro.win;
    ro.win = lj_window_create(&lj_form_TREORDERCUEPAGESFORM, b);
    if (!ro.win) { lj_log("cue pages: TReorderCuePagesForm failed to build"); return NULL; }
    ro.work = *lj_cue_pages_model();
    if ((w = lj_window_find(ro.win, "lvPages")) != NULL) { ro.lv_event = w->on_event; ro.lv_ud = w->ud; w->on_event = ro_lv_event; }
    if ((w = lj_window_find(ro.win, "bOk")) != NULL) { w->on_event = ro_ok; w->ud = NULL; }
    if ((w = lj_window_find(ro.win, "bCancel")) != NULL) { w->on_event = ro_cancel; w->ud = NULL; }
    ro_fill();
    lj_win_show(ro.win, 0);
    return ro.win;
}

/* ---- hotkeys [01 s7 Cue rows] ---- */
static void fn_pages(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (!edge) return;
    switch (fn) {
    case LJ_FN_PREVIOUS_CUE_PAGE:         lj_cue_pages_step_page(-1); break;
    case LJ_FN_NEXT_CUE_PAGE:             lj_cue_pages_step_page(1); break;
    case LJ_FN_CUE_PAGE_LATCH_PREVIOUS:   lj_cue_pages_step_cue(-1, 1); break;
    case LJ_FN_CUE_PAGE_LATCH_NEXT:       lj_cue_pages_step_cue(1, 1); break;
    case LJ_FN_CUE_PAGE_SELECT_PREVIOUS:  lj_cue_pages_step_cue(-1, 0); break;
    case LJ_FN_CUE_PAGE_SELECT_NEXT:      lj_cue_pages_step_cue(1, 0); break;
    case LJ_FN_LOAD_SELECTED_CUE_PAGE: {
        lj_cuepages *p = lj_cue_pages_model();
        int i = lv_index() >= 0 ? lv_index() : p->cursor;
        if (i >= 0 && i < p->page[p->current].n) load(p->page[p->current].cue[i]);
        break;
    }
    default: break;
    }
}
void lj_cue_pages_bind_fns(void)
{
    static const enum lj_fn fns[] = { LJ_FN_PREVIOUS_CUE_PAGE, LJ_FN_NEXT_CUE_PAGE, LJ_FN_CUE_PAGE_LATCH_PREVIOUS, LJ_FN_CUE_PAGE_LATCH_NEXT,
                                      LJ_FN_CUE_PAGE_SELECT_PREVIOUS, LJ_FN_CUE_PAGE_SELECT_NEXT, LJ_FN_LOAD_SELECTED_CUE_PAGE };
    int i;
    for (i = 0; i < LJ_COUNT_OF(fns); i++) lj_fn_bind(fns[i], fn_pages, NULL);
}
