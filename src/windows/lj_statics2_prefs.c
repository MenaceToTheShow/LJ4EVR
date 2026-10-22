/* lj_statics2_prefs.c - see lj_statics2_prefs.h. Every OnXxx of TSTATICS2PREFSFORM, TGETSTATICSPAGENAMEFORM and
 * TSTATICENTRYPROPERTIESFORM is bound (ME-16). */
#include "windows/lj_statics2_prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_locale.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "model/lj_statics.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_udbutton.h"
#include "ui/widgets/lj_w_updown.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_statics2_form.h"
#include "windows/lj_windows.h"

/* the TAction captions of TStatics2Form's ActionList, in enum lj_replace order (D13) */
static const char *const lj_sp_rep[] = { "Add", "Replace Slot", "Replace Page", "Replace Group", "Replace All", "Solo Slot", "Solo Page", "Solo Group", "Solo All" };
static const char *const lj_sp_act[] = { "Toggle", "Flash" };

static lj_static_groups lj_sp_own_groups;
static struct {
    lj_window *win, *name_win;
    int name_mode;                                     /* 0 Add, 1 Insert, 2 Edit name */
    int guard, close_pending;
    char page_row[LJ_STATIC_PAGES_MAX][72], slot_row[LJ_STATICS2_SLOTS_MAX][96], grp_row[LJ_STATIC_GROUPS][76], count[24];
    const char *page_items[LJ_STATIC_PAGES_MAX], *slot_items[LJ_STATICS2_SLOTS_MAX], *grp_items[LJ_STATIC_GROUPS];
} sp;

static lj_ctx *C(void) { return lj_ctx_get(); }
static lj_widget *find(const char *n) { return sp.win ? lj_window_find(sp.win, n) : NULL; }
static int lv_index(lj_widget *lv) { return lv ? ((lj_w_listview *)lv)->index : -1; }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

static lj_static_groups *groups(void)                  /* the names live in Statics.dat ("group n name="); loaded once */
{
    lj_ctx *c = C();
    char path[LJ_PATH_CAP], err[128];
    int i;
    if (c->static_groups) return c->static_groups;
    FILE *f = c->lib && lj_library_path(c->lib, "Statics.dat", path, sizeof path) == 0 ? fopen(path, "rb") : NULL;
    int there = f != NULL;
    if (f) fclose(f);
    if (there && lj_statics_groups_load(path, &lj_sp_own_groups, err, sizeof err) != 0) lj_log("statics: %s", err);   /* a bad file keeps the defaults it filled */
    for (i = 0; !there && i < LJ_STATIC_GROUPS; i++) snprintf(lj_sp_own_groups.name[i], sizeof lj_sp_own_groups.name[i], "Group %d", i + 1);   /* [14 sB.3] vocabulary */
    c->static_groups = &lj_sp_own_groups;
    return c->static_groups;
}

static void set_edit_int(lj_widget *e, int v) { char t[16]; snprintf(t, sizeof t, "%d", v); lj_w_edit_set_text(e, t); }
static int edit_int(lj_widget *e, int dflt) { const char *t = lj_w_edit_text(e); char *end; long v; if (!t || !*t) return dflt; v = strtol(t, &end, 10); return *end ? dflt : (int)v; }

/* ---- the lists ---- */
static void fill_pages(void)
{
    lj_show *sh = C()->show;
    int i, n = sh ? sh->npages : 0;
    for (i = 0; i < n; i++) {
        if (sh->pages[i].name[0]) lj_str_copy(sp.page_row[i], sizeof sp.page_row[i], sh->pages[i].name);
        else snprintf(sp.page_row[i], sizeof sp.page_row[i], "Page %d", i + 1);
        sp.page_items[i] = sp.page_row[i];
    }
    lj_w_listview_set_items(find("lvPages"), sp.page_items, n);
    snprintf(sp.count, sizeof sp.count, "%d pages", n);                 /* exe "%d pages" [14 sB.3] */
    lj_w_set_caption(find("lPages"), sp.count);
}
static lj_static_slot *cur_slot(int i)
{
    lj_show *sh = C()->show;
    int p = lj_statics2_page();
    return sh && p >= 1 && p <= sh->npages && i >= 0 && i < LJ_STATICS2_SLOTS_MAX ? &sh->pages[p - 1].slot[i] : NULL;
}
static void fill_slots(void)
{
    int i, n = lj_statics2_visible();
    for (i = 0; i < n; i++) {
        lj_static_slot *st = cur_slot(i);
        snprintf(sp.slot_row[i], sizeof sp.slot_row[i], "%d\t%s\t%s", i + 1, st && st->transparent ? "Yes" : "", st ? st->type : "");   /* "Yes": PROPOSAL rig 14 sB.8 item 6 */
        sp.slot_items[i] = sp.slot_row[i];
    }
    lj_w_listview_set_items(find("lvSlotTypes"), sp.slot_items, n);
}
static void fill_groups(void)
{
    lj_static_groups *g = groups();
    int i;
    for (i = 0; i < LJ_STATIC_GROUPS; i++) { snprintf(sp.grp_row[i], sizeof sp.grp_row[i], "%d\t%s", i + 1, g->name[i]); sp.grp_items[i] = sp.grp_row[i]; }
    lj_w_listview_set_items(find("lvGroups"), sp.grp_items, LJ_STATIC_GROUPS);
}

/* ---- the page operations (lj_statics2_prefs.h): Insert / Delete / Up / Down release every static first ---- */
enum { OP_ADD, OP_INSERT, OP_RENAME, OP_DELETE, OP_UP, OP_DOWN };
static int pages_edit(int op, int at, const char *name)
{
    lj_ctx *c = C();
    lj_show *sh = c->show;
    lj_cmd cmd;
    int r = -1, i;
    if (!sh) return -1;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_STATIC_DEACTIVATE; cmd.a = LJ_RUN_ALL; cmd.c = 0; cmd.at_us = lj_clock_us();   /* Release All */
    lj_lock();
    if (op != OP_ADD && op != OP_RENAME && c->engine) lj_engine_cmd(c->engine, &cmd);
    switch (op) {
    case OP_ADD:    r = lj_statics_insert_page(&sh->pages, &sh->npages, sh->npages, name); break;
    case OP_INSERT: r = lj_statics_insert_page(&sh->pages, &sh->npages, at < 0 ? 0 : at, name); break;
    case OP_RENAME: if (at >= 0 && at < sh->npages) { lj_str_copy(sh->pages[at].name, sizeof sh->pages[at].name, name); r = at; } break;
    case OP_DELETE: r = lj_statics_delete_page(sh->pages, &sh->npages, at) == 0 ? (at < sh->npages ? at : sh->npages - 1) : -1; break;
    case OP_UP:     r = lj_statics_swap_pages(sh->pages, sh->npages, at, at - 1) == 0 ? at - 1 : -1; break;
    case OP_DOWN:   r = lj_statics_swap_pages(sh->pages, sh->npages, at, at + 1) == 0 ? at + 1 : -1; break;
    default: break;
    }
    if (r >= 0 && (op == OP_ADD || op == OP_INSERT)) {
        sh->pages[r].n = (uint16_t)(c->static_visible > 0 ? c->static_visible : LJ_STATICS2_SLOTS_PER_PAGE);
        for (i = 0; i < LJ_STATICS2_SLOTS_MAX; i++) sh->pages[r].slot[i].level_pct = LJ_STATIC_LEVEL_DEFAULT_PCT;   /* [STATICS] DefaultLevel=255 */
    }
    lj_unlock();
    if (r < 0) { lj_log("statics: page not changed (%d pages max)", LJ_STATIC_PAGES_MAX); return -1; }
    lj_statics2_save();
    if (lj_statics2_set_page(r + 1) != 0) lj_statics2_set_page(1);         /* the Statics window's page buttons follow */
    fill_pages(); fill_slots();
    lj_w_listview_select(find("lvPages"), r);
    return r;
}

/* ---- TGetStaticsPageNameForm ---- */
lj_window *lj_statics2_page_name_window(void) { return sp.name_win; }
static void name_close(void) { if (sp.name_win) { lj_win_hide(sp.name_win); lj_window_destroy(sp.name_win); sp.name_win = NULL; } }
static void name_ok(lj_widget *w, int ev, void *ud)
{
    const char *name;
    int at = lv_index(find("lvPages")), op = sp.name_mode == 0 ? OP_ADD : sp.name_mode == 1 ? OP_INSERT : OP_RENAME;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK || !sp.name_win) return;
    name = lj_w_edit_text(lj_window_find(sp.name_win, "eName"));
    if (op != OP_ADD && at < 0) at = lj_statics2_page() - 1;
    pages_edit(op, at, name ? name : "");
    name_close();
}
static void name_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) name_close(); }
static void name_open(int mode)
{
    static const lj_binding b[] = { { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { NULL, NULL, NULL } };
    static const char *const caps[3] = { "Add Page", "Insert Page", "Edit Pagename" };   /* exe pool [14 sB.3]; which caption for which button: PROPOSAL rig 14 sB.8 item 12 */
    lj_show *sh = C()->show;
    lj_widget *w;
    int at = lv_index(find("lvPages"));
    name_close();
    if ((sp.name_win = lj_window_create(&lj_form_TGETSTATICSPAGENAMEFORM, b)) == NULL) { lj_log("statics: cannot create TGetStaticsPageNameForm"); return; }
    sp.name_mode = mode;
    lj_window_set_caption(sp.name_win, caps[mode]);
    if ((w = lj_window_find(sp.name_win, "eName")) != NULL) {
        lj_w_edit_set_text(w, mode == 2 && sh && at >= 0 && at < sh->npages ? sp.page_row[at] : "New Page");   /* exe "New Page" */
        lj_w_edit_select(w, 0, -1); lj_widget_set_focus(w);
    }
    if ((w = lj_window_find(sp.name_win, "BitBtn1")) != NULL) { w->on_event = name_ok; w->ud = NULL; }       /* bkOK / bkCancel carry no OnClick in the DFM */
    if ((w = lj_window_find(sp.name_win, "BitBtn2")) != NULL) { w->on_event = name_cancel; w->ud = NULL; }
    lj_win_show(sp.name_win, 0);
}

/* ---- TStatics2PrefsForm ---- */
static void prefs_fill(void)
{
    lj_statics2_view v;
    lj_statics2_view_get(&v);
    sp.guard = 1;
    lj_w_checkbox_set_checked(find("cbViewSlot"), v.slot_no_visible);
    lj_w_checkbox_set_checked(find("cbViewMode"), v.mode_visible);
    lj_w_checkbox_set_checked(find("cbViewLevels"), v.lvl_visible);
    set_edit_int(find("eRows"), v.slots_visible);
    lj_w_updown_set_position(find("udRows"), v.slots_visible);
    lj_w_checkbox_set_checked(find("cbTabsMultiLine"), v.multiline);
    set_edit_int(find("eTabsWidth"), v.tab_width); set_edit_int(find("eTabsHeight"), v.tab_height);
    lj_w_checkbox_set_checked(find("cbTabsAutoWidth"), v.tab_width == 0);
    lj_w_checkbox_set_checked(find("cbTabsAutoHeight"), v.tab_height == 0);
    sp.guard = 0;
}
static int prefs_apply(void)
{
    lj_statics2_view v;
    if (!sp.win) return -1;
    lj_statics2_view_get(&v);
    v.slot_no_visible = lj_w_checkbox_checked(find("cbViewSlot"));
    v.mode_visible = lj_w_checkbox_checked(find("cbViewMode"));
    v.lvl_visible = lj_w_checkbox_checked(find("cbViewLevels"));
    v.slots_visible = edit_int(find("eRows"), v.slots_visible);              /* clamped 1..64 by lj_statics2_view_set */
    v.multiline = lj_w_checkbox_checked(find("cbTabsMultiLine"));
    v.tab_width = lj_w_checkbox_checked(find("cbTabsAutoWidth")) ? 0 : edit_int(find("eTabsWidth"), 0);
    v.tab_height = lj_w_checkbox_checked(find("cbTabsAutoHeight")) ? 0 : edit_int(find("eTabsHeight"), 0);
    if (v.tab_width < 0 || v.tab_width > 1023) v.tab_width = 0;
    if (v.tab_height < 0 || v.tab_height > 1023) v.tab_height = 0;
    lj_statics2_view_set(&v);
    lj_statics2_save();
    prefs_fill(); fill_slots();
    return 0;
}
static void b_click(lj_widget *w, void *ud)
{
    const char *n = w && w->name ? w->name : "";
    (void)ud;
    if (!strcmp(n, "bOk")) { prefs_apply(); sp.close_pending = 1; }
    else if (!strcmp(n, "bApply")) prefs_apply();
    else sp.close_pending = 1;                                             /* bCancel */
}
static void set_default(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    sp.guard = 1;
    lj_w_checkbox_set_checked(find("cbViewSlot"), 1); lj_w_checkbox_set_checked(find("cbViewMode"), 1); lj_w_checkbox_set_checked(find("cbViewLevels"), 1);
    set_edit_int(find("eRows"), LJ_STATICS2_SLOTS_PER_PAGE);
    lj_w_updown_set_position(find("udRows"), LJ_STATICS2_SLOTS_PER_PAGE);
    sp.guard = 0;
}
static void page_button(lj_widget *w, void *ud)
{
    const char *n = w && w->name ? w->name : "";
    lj_widget *lv = find("lvPages"), *lg = find("lvGroups");
    int at = lv_index(lv), g = lv_index(lg), p, i;
    (void)ud;
    if (!strcmp(n, "sbAddPage")) name_open(0);
    else if (!strcmp(n, "sbInsertPage")) name_open(1);
    else if (!strcmp(n, "sbEditPage")) { if (at >= 0) name_open(2); }
    else if (!strcmp(n, "sbDeletePage")) { if (at >= 0) pages_edit(OP_DELETE, at, NULL); }
    else if (!strcmp(n, "sbUp")) { if (at > 0) pages_edit(OP_UP, at, NULL); }
    else if (!strcmp(n, "sbDown")) { if (at >= 0) pages_edit(OP_DOWN, at, NULL); }
    else if (!strcmp(n, "sbAddGroup")) {                                   /* the first group still on its default name, ready to be named */
        lj_static_groups *gr = groups();
        char d[16];
        for (i = 0; i < LJ_STATIC_GROUPS; i++) { snprintf(d, sizeof d, "Group %d", i + 1); if (!strcmp(gr->name[i], d)) break; }
        if (i < LJ_STATIC_GROUPS) { lj_w_listview_select(lg, i); lj_widget_set_focus(find("eGroupName")); }
    } else if (!strcmp(n, "sbDeleteGroup") && g >= 0 && g < LJ_STATIC_GROUPS) {
        lj_show *sh = C()->show;
        snprintf(groups()->name[g], sizeof groups()->name[g], "Group %d", g + 1);   /* [14 sB.3] vocabulary */
        lj_lock();
        for (p = 0; sh && p < sh->npages; p++) for (i = 0; i < LJ_STATICS2_SLOTS_MAX; i++) sh->pages[p].slot[i].groups &= ~((uint64_t)1 << g);
        lj_unlock();
        lj_statics2_save(); fill_groups();
    }
}
static void toggle_transparent(lj_widget *w, void *ud)
{
    lj_static_slot *st = cur_slot(lv_index(find("lvSlotTypes")));
    (void)w; (void)ud;
    if (!st) return;
    lj_lock(); st->transparent = (uint8_t)!st->transparent; lj_unlock();
    lj_statics2_save(); fill_slots();
}
static void edit_change(lj_widget *w, void *ud)                            /* eSlotType / eGroupName: the selected row's text */
{
    const char *t = lj_w_edit_text(w);
    int i;
    (void)ud;
    if (sp.guard || !t || !w->name) return;
    if (!strcmp(w->name, "eSlotType")) {
        lj_static_slot *st = cur_slot(lv_index(find("lvSlotTypes")));
        if (!st) return;
        lj_lock(); lj_str_copy(st->type, sizeof st->type, t); lj_unlock();
        i = lv_index(find("lvSlotTypes")); fill_slots(); lj_w_listview_select(find("lvSlotTypes"), i);
    } else if ((i = lv_index(find("lvGroups"))) >= 0 && i < LJ_STATIC_GROUPS) {
        lj_str_copy(groups()->name[i], sizeof groups()->name[i], t);
        fill_groups(); lj_w_listview_select(find("lvGroups"), i);
    } else return;
    lj_statics2_save();
}
static void list_change(lj_widget *w, void *ud)                            /* lvSlotTypes / lvGroups: the edit shows the selected row */
{
    int i = lv_index(w);
    (void)ud;
    if (sp.guard || i < 0 || !w->name) return;
    sp.guard = 1;
    if (!strcmp(w->name, "lvSlotTypes")) { lj_static_slot *st = cur_slot(i); lj_w_edit_set_text(find("eSlotType"), st ? st->type : ""); }
    else if (i < LJ_STATIC_GROUPS) lj_w_edit_set_text(find("eGroupName"), groups()->name[i]);
    sp.guard = 0;
}
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; sp.close_pending = 1; }

static const lj_binding lj_sp_bindings[] = {
    { "FormActivate", noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormShow", noop, NULL },
    { "PageControl1Change", noop, NULL }, { "sbDefaultClick", set_default, NULL }, { "ETabsSizeChange", noop, NULL } /* read on Apply */,
    { "sbPageButtonClick", page_button, NULL }, { "sbPageMouseDown", noop, NULL }, { "lvPagesChange", noop, NULL }, { "lvPagesData", noop, NULL },
    { "lvPagesDragDrop", noop, NULL }, { "lvPagesDragOver", noop, NULL } /* ponytail: Up / Down reorder; drag when asked */,
    { "sbToggleTransparentClick", toggle_transparent, NULL }, { "eChange", edit_change, NULL }, { "lvChange", list_change, NULL },
    { "bClick", b_click, NULL }, { NULL, NULL, NULL } };

lj_window *lj_statics2_prefs_open(void)
{
    if (sp.win) return sp.win;
    if ((sp.win = lj_window_create(&lj_form_TSTATICS2PREFSFORM, lj_sp_bindings)) == NULL) { lj_log("statics: cannot create TStatics2PrefsForm"); return NULL; }
    sp.close_pending = 0;

    lj_w_updown_bind(find("udRows"), find("eRows"), lj_w_edit_set_text);
    lj_w_updown_bind(find("udTabsWidth"), find("eTabsWidth"), lj_w_edit_set_text);
    lj_w_updown_bind(find("udTabsHeight"), find("eTabsHeight"), lj_w_edit_set_text);
    prefs_fill();
    fill_pages(); fill_slots(); fill_groups();
    lj_w_tabs_select(find("PageControl1"), 0);                                /* DFM ActivePage = tbGroups is the designer's */
    lj_win_show(sp.win, 2u /* LJ_WF_STAY_ON_TOP */);
    return sp.win;
}

void lj_statics2_prefs_close(void)
{
    name_close();
    if (!sp.win) return;
    lj_win_hide(sp.win);
    lj_window_destroy(sp.win);
    sp.win = NULL; sp.close_pending = 0;
}
lj_window *lj_statics2_prefs_window(void) { return sp.win; }
void lj_statics2_prefs_refresh(void) { if (sp.close_pending) lj_statics2_prefs_close(); }

/* ---- TStaticEntryPropertiesForm ---- */
static struct { lj_window *win; int page, slot, close_pending, fade_ms; char t[3][96], fade[32], count[24]; const char *items[LJ_STATIC_GROUPS]; } pr;
static lj_widget *pfind(const char *n) { return pr.win ? lj_window_find(pr.win, n) : NULL; }
static lj_static_slot *props_slot(void)
{
    lj_show *sh = C()->show;
    return sh && pr.page >= 1 && pr.page <= sh->npages && pr.slot >= 1 && pr.slot <= LJ_STATICS2_SLOTS_MAX ? &sh->pages[pr.page - 1].slot[pr.slot - 1] : NULL;
}
static void props_fade_text(void)
{
    snprintf(pr.fade, sizeof pr.fade, "%d%c%d", pr.fade_ms / 1000, lj_locale_decimal(), pr.fade_ms % 1000 / 100);   /* seconds, one decimal, the host separator (D6) */
    lj_w_edit_set_text(pfind("eFadeTimeDummy"), pr.fade);
}
static void props_count(void)
{
    lj_widget *lb = pfind("lbGroups");
    int i, n = 0;
    for (i = 0; lb && i < LJ_STATIC_GROUPS; i++) n += lj_w_list_checked(lb, i) != 0;
    snprintf(pr.count, sizeof pr.count, "%d Groups", n);                     /* exe "%d Groups" [14 sB.5] */
    lj_w_set_caption(pfind("lGroupCount"), pr.count);
}
static int props_apply(void)
{
    lj_static_slot *st = props_slot();
    lj_widget *lb = pfind("lbGroups");
    const char *name = lj_w_edit_text(pfind("eName"));
    int act = ((lj_w_combobox *)pfind("cbActivation"))->index, rep = ((lj_w_combobox *)pfind("cbReplaceMode"))->index, i;
    uint64_t g = 0;
    if (!st) return -1;
    for (i = 0; lb && i < LJ_STATIC_GROUPS; i++) if (lj_w_list_checked(lb, i)) g |= (uint64_t)1 << i;
    lj_lock();                                                                 /* the engine reads replace / groups of the slots it holds */
    if (name) lj_str_copy(st->name, sizeof st->name, name);
    if (act >= 0 && act <= LJ_ACT_FLASH) st->activation = (uint8_t)act;
    if (rep >= 0 && rep <= LJ_REP_SOLO_ALL) st->replace = (uint8_t)rep;
    st->fade_ms = (uint32_t)pr.fade_ms; st->groups = g;
    lj_unlock();
    lj_statics2_save();
    lj_statics2_set_page(lj_statics2_page());                                  /* the slot button shows the new name */
    return 0;
}
static void props_b(lj_widget *w, void *ud)
{
    const char *n = w && w->name ? w->name : "";
    (void)ud;
    if (!strcmp(n, "bOk")) { props_apply(); pr.close_pending = 1; }
    else if (!strcmp(n, "bApply")) props_apply();
    else pr.close_pending = 1;
}
static void props_grp(lj_widget *w, void *ud)                                  /* sbSetGrpAll / sbGrpClear */
{
    lj_widget *lb = pfind("lbGroups");
    int i, on = w && w->name && !strcmp(w->name, "sbSetGrpAll");
    (void)ud;
    for (i = 0; lb && i < LJ_STATIC_GROUPS; i++) lj_w_list_set_checked(lb, i, on);
    props_count();
}
static void props_fade(lj_widget *w, void *ud)                                 /* UdFade: 0.1 s a step, 0..60 s */
{
    (void)ud;
    pr.fade_ms += 100 * lj_w_udbutton_delta(w);
    pr.fade_ms = pr.fade_ms < 0 ? 0 : pr.fade_ms > LJ_TIME_MAX_MS ? LJ_TIME_MAX_MS : pr.fade_ms;
    props_fade_text();
}
static void props_on_close(lj_widget *w, void *ud) { (void)w; (void)ud; pr.close_pending = 1; }
static const lj_binding lj_pr_bindings[] = {
    { "FormClose", props_on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormResize", noop, NULL },
    { "bClick", props_b, NULL }, { "sbGrpClick", props_grp, NULL }, { "UdFadeChange", props_fade, NULL }, { NULL, NULL, NULL } };

lj_window *lj_static_props_open(void)
{
    lj_static_groups *g = groups();
    lj_static_slot *st;
    lj_widget *lb;
    int i;
    lj_static_props_close();
    pr.page = lj_statics2_page(); pr.slot = lj_statics2_popup_slot() > 0 ? lj_statics2_popup_slot() : 1;   /* the last popup's slot; slot 1 before any (the UI script's `open static_props`) */
    if ((st = props_slot()) == NULL) { lj_log("statics: Properties: no Statics 2 page"); return NULL; }
    if ((pr.win = lj_window_create(&lj_form_TSTATICENTRYPROPERTIESFORM, lj_pr_bindings)) == NULL) { lj_log("statics: cannot create TStaticEntryPropertiesForm"); return NULL; }
    pr.close_pending = 0;
    snprintf(pr.t[0], sizeof pr.t[0], "Entry: %s", st->name[0] ? st->name : "");          /* exe "Entry: %s" / "Page: %s" / "Type: %s" [14 sB.5] */
    snprintf(pr.t[1], sizeof pr.t[1], "Page: %s", C()->show->pages[pr.page - 1].name[0] ? C()->show->pages[pr.page - 1].name : "");
    snprintf(pr.t[2], sizeof pr.t[2], "Type: %s", st->type);
    lj_w_set_caption(pfind("lSlot"), pr.t[0]); lj_w_set_caption(pfind("lPage"), pr.t[1]); lj_w_set_caption(pfind("lType"), pr.t[2]);
    lj_w_edit_set_text(pfind("eName"), st->name);
    lj_w_combobox_set_items(pfind("cbActivation"), lj_sp_act, 2);
    lj_w_combobox_set_items(pfind("cbReplaceMode"), lj_sp_rep, LJ_COUNT_OF(lj_sp_rep));
    if (pfind("cbActivation")) ((lj_w_combobox *)pfind("cbActivation"))->index = st->activation <= LJ_ACT_FLASH ? st->activation : 0;
    if (pfind("cbReplaceMode")) ((lj_w_combobox *)pfind("cbReplaceMode"))->index = st->replace <= LJ_REP_SOLO_ALL ? st->replace : 0;
    pr.fade_ms = (int)(st->fade_ms > LJ_TIME_MAX_MS ? LJ_TIME_MAX_MS : st->fade_ms);
    props_fade_text();
    for (i = 0; i < LJ_STATIC_GROUPS; i++) pr.items[i] = g->name[i];
    if ((lb = pfind("lbGroups")) != NULL) {
        lj_w_list_set_items(lb, pr.items, LJ_STATIC_GROUPS);
        for (i = 0; i < LJ_STATIC_GROUPS; i++) lj_w_list_set_checked(lb, i, (int)((st->groups >> i) & 1u));
    }
    props_count();
    lj_win_show(pr.win, 2u /* LJ_WF_STAY_ON_TOP */);
    return pr.win;
}
void lj_static_props_close(void)
{
    if (!pr.win) return;
    lj_win_hide(pr.win);
    lj_window_destroy(pr.win);
    pr.win = NULL; pr.close_pending = 0;
}
lj_window *lj_static_props_window(void) { return pr.win; }
void lj_static_props_refresh(void) { if (pr.close_pending) lj_static_props_close(); }
