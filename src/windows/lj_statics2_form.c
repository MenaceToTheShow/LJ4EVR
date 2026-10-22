/* lj_statics2_form.c - the Statics 2 window (lj_statics2_form.h). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_statics2_form.h"

#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "model/lj_statics.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_w_slotbtn.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_windows.h"

#define LJ_S2_STAY_ON_TOP 2u            /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here) */
#define LJ_S2_ON_RGB      0x00FF00u     /* an active slot: green, the old control's LED colour (07 s4.4 LJ_LED_ON_RGB); PROPOSAL rig 14 sB.8 item 12 */
#define LJ_S2_LEVEL_RGB   0xFFFFFFu     /* the white level cell of staticcontrol.gif */
#define LJ_S2_N           LJ_STATICS2_SLOTS_MAX   /* ME-30: Visible Slots up to 64 rows */

/* enum lj_replace order: the TAction captions (DFM ActionList1) */
static const char *const lj_s2_rep_word[] = { "Add", "Replace Slot", "Replace Page", "Replace Group", "Replace All", "Solo Slot", "Solo Page", "Solo Group", "Solo All" };
static const char *const lj_s2_rep_action[] = { "rpAdd", "rpSlot", "rpPage", "rpGroup", "rpAll", "rpSoloSlot", "rpSoloPage", "rpSoloGroup", "rpSoloAll" };
static const lj_ctl_desc lj_s2_label_desc = { LJ_K_LABEL, "lj_s2_number", 0, 0, 0, 0, .alignment = LJ_TA_RIGHT };   /* lj_w_label paints from a desc */

static int lj_s2_cur_page = 1;          /* survives close: the hotkeys address the current page */
static struct {
    lj_window *win;
    lj_widget *num[LJ_S2_N], *btn[LJ_S2_N], *mode[LJ_S2_N], *lvl[LJ_S2_N];
    int idx[LJ_S2_N], n;
    char numtxt[LJ_S2_N][12], lvltxt[LJ_S2_N][16], tabtxt[LJ_STATIC_PAGES_MAX][20];
    const char *tabs[LJ_STATIC_PAGES_MAX];
    int pop_slot, drag;
    lj_cmd last; int has_last;
} s;

lj_window *lj_statics2_form_get(void) { return s.win; }
int lj_statics2_page(void) { return lj_s2_cur_page; }
int lj_statics2_popup_slot(void) { return s.pop_slot; }
lj_widget *lj_statics2_slot_button(int slot) { return slot >= 1 && slot <= s.n ? s.btn[slot - 1] : NULL; }
int lj_statics2_last_cmd(lj_cmd *out) { if (out && s.has_last) *out = s.last; return s.has_last; }

static lj_show *show(void) { return lj_ctx_get()->show; }
static int npages(void) { lj_show *sh = show(); return sh && sh->pages ? sh->npages : 0; }
static lj_static_slot *slot_ptr(int slot)
{
    lj_show *sh = show();
    if (slot < 1 || slot > LJ_S2_N || lj_s2_cur_page < 1 || lj_s2_cur_page > npages()) return NULL;
    return &sh->pages[lj_s2_cur_page - 1].slot[slot - 1];
}
static int statics_geti(const char *key) { lj_ctx *c = lj_ctx_get(); return c->main_ini ? lj_ini_geti(c->main_ini, "STATICS", key, 0) : 0; }

int lj_statics2_visible(void)
{
    lj_ctx *c = lj_ctx_get();
    int n = lj_s2_cur_page >= 1 && lj_s2_cur_page <= npages() ? c->show->pages[lj_s2_cur_page - 1].n : 0;
    if (n < 1) n = c->static_visible;
    if (n < 1) n = LJ_S2_N;
    return n > LJ_S2_N ? LJ_S2_N : n;                         /* ME-30: 1..64 */
}

int lj_statics2_slot_active(int slot)
{
    const lj_engine_status *v = &lj_ctx_get()->view;
    int g = lj_static_global_index(lj_s2_cur_page, slot), r;
    if (g < 0) return 0;
    for (r = 0; r < LJ_STATIC_RUNNERS; r++) if (v->static_active[r] && v->static_slot_of[r] == (uint16_t)g) return 1;
    return 0;
}

static void post(lj_cmd *cmd) { s.last = *cmd; s.has_last = 1; lj_ctx_post(cmd); }
static void activate(int slot, lj_static_slot *st, int scope)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_STATIC_ACTIVATE; cmd.a = (uint32_t)lj_static_global_index(lj_s2_cur_page, slot);
    cmd.b = st->level_pct; cmd.c = (uint32_t)scope; cmd.st = st;
    post(&cmd);
}
static void deactivate(int slot)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_STATIC_DEACTIVATE; cmd.a = (uint32_t)lj_static_global_index(lj_s2_cur_page, slot);
    post(&cmd);
}

int lj_statics2_press(int slot, int down)
{
    lj_static_slot *st = slot_ptr(slot);
    int active = lj_statics2_slot_active(slot);
    if (!st || st->kind == LJ_STATIC_EMPTY) return -1;
    if (st->activation == LJ_ACT_FLASH) {                       /* active while held [03 s8] */
        if (down && !active) activate(slot, st, st->replace);
        else if (!down) deactivate(slot);
        return 0;
    }
    if (!down) return -1;
    if (active) deactivate(slot); else activate(slot, st, st->replace);
    return 0;
}

void lj_statics2_release(int page)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_STATIC_DEACTIVATE; cmd.a = LJ_RUN_ALL; cmd.c = (uint32_t)page;   /* c = 0 Release All, c = page Release Page (lj_engine.h table) */
    post(&cmd);
}

static void level_text(int i)
{
    lj_static_slot *st = slot_ptr(i + 1);
    snprintf(s.lvltxt[i], sizeof s.lvltxt[i], " Level %u%%", st ? (unsigned)st->level_pct : 0u);   /* exe " Level %0.0f%%" [14 sB.6] */
    if (s.lvl[i]) lj_w_slotbtn_set(s.lvl[i], s.lvltxt[i], 0);
}

int lj_statics2_set_level(int slot, int pct)
{
    lj_static_slot *st = slot_ptr(slot);
    int active;
    if (!st) return -1;
    pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    lj_lock(); st->level_pct = (uint8_t)pct; lj_unlock();
    active = lj_statics2_slot_active(slot);
    if (st->kind != LJ_STATIC_EMPTY) {
        if (active && pct == 0 && statics_geti("LevelAutoDisable")) deactivate(slot);
        else if (active) activate(slot, st, LJ_REP_ADD);          /* the new level, no scope releases */
        else if (pct > 0 && statics_geti("LevelAutoEnable")) activate(slot, st, st->replace);
    }
    if (slot <= s.n) level_text(slot - 1);
    return 0;
}

/* ---- page tabs and rows ---- */
static void fill_rows(void)
{
    int i;
    for (i = 0; i < s.n; i++) {
        lj_static_slot *st = slot_ptr(i + 1);
        lj_w_slotbtn_set(s.btn[i], st ? st->name : "", lj_statics2_slot_active(i + 1));
        lj_w_slotbtn_set(s.mode[i], st ? lj_s2_rep_word[st->replace <= LJ_REP_SOLO_ALL ? st->replace : 0] : "", 0);
        level_text(i);
    }
}
static void fill_tabs(void)
{
    lj_widget *tb = lj_window_find(s.win, "tbPages");
    lj_show *sh = show();
    int i, n = npages();
    for (i = 0; i < n; i++) {
        snprintf(s.tabtxt[i], sizeof s.tabtxt[i], "Page %d", i + 1);   /* exe "Page 1" default name [14 sB.3] */
        s.tabs[i] = sh->pages[i].name[0] ? sh->pages[i].name : s.tabtxt[i];
    }
    if (!tb) return;
    ((lj_w_tabs *)tb)->tabs = s.tabs; ((lj_w_tabs *)tb)->ntabs = n;
    ((lj_w_tabs *)tb)->active = lj_s2_cur_page - 1;
    lj_w_tabs_layout(tb);
    lj_widget_invalidate(tb);
}

int lj_statics2_set_page(int page)
{
    if (page < 1 || page > npages()) return -1;
    lj_s2_cur_page = page;
    if (s.win) { fill_tabs(); fill_rows(); }
    return 0;
}

void lj_statics2_form_refresh(void)
{
    const lj_engine_status *v = &lj_ctx_get()->view;
    int i;
    if (v->tick_no) lj_desktop_led_set(LJ_LED_STATICS, v->statics_any);   /* cue-list toolbar LED green when any entry is active [03 s8] */
    if (!s.win) return;
    for (i = 0; i < s.n; i++) {
        int on = lj_statics2_slot_active(i + 1);
        uint32_t rgb = on ? LJ_S2_ON_RGB : LJ_CLR_BTNFACE;
        lj_static_slot *st = slot_ptr(i + 1);
        if (s.btn[i]->color != rgb) s.btn[i]->color = rgb;
        lj_w_slotbtn_set(s.btn[i], st ? st->name : "", on);
    }
}

/* ---- the popup and its TAction items ---- */
static int rep_of(const char *action)
{
    int i;
    for (i = 0; action && i < (int)LJ_COUNT_OF(lj_s2_rep_action); i++) if (!strcmp(action, lj_s2_rep_action[i])) return i;
    return -1;
}
static void sync_popup(void)
{
    lj_menu *m = lj_window_menus(s.win);
    lj_static_slot *st = slot_ptr(s.pop_slot);
    int i, active = lj_statics2_slot_active(s.pop_slot);
    for (i = 0; m && i < m->nrt; i++) {
        const char *a = m->rt[i].item->action;
        if (!a || !st) continue;
        if (rep_of(a) >= 0) m->rt[i].checked = rep_of(a) == st->replace;
        else if (!strcmp(a, "acToggle")) m->rt[i].checked = st->activation == LJ_ACT_TOGGLE;
        else if (!strcmp(a, "acFlash")) m->rt[i].checked = st->activation == LJ_ACT_FLASH;
    }
    lj_menu_set_enabled(m, "M_RecordCurrentScene", !active);   /* "only while the entry is released" [03 s8] */
    lj_menu_set_enabled(m, "M_LinkToCurrentScene", !active);
    lj_menu_set_enabled(m, "M_ConvertToScene", st && st->kind == LJ_STATIC_RECORDED && st->scene);
}

static void open_menu(const char *name, int slot, int x, int y)
{
    lj_menu *m = lj_window_menus(s.win);
    int sx = 0, sy = 0;
    if (!m || !slot_ptr(slot)) return;
    s.pop_slot = slot;
    sync_popup();
    if (m->host && m->host->screen_pos) m->host->screen_pos(m->host->ud, s.win, &sx, &sy);
    lj_menu_popup(m, lj_form_find_menu(lj_window_desc(s.win), name), sx + x, sy + y);
}
void lj_statics2_popup(int slot, int x, int y) { if (s.win) open_menu("popStaticEntry", slot, x, y); }

static void save_statics(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!c->lib || !c->show || lj_library_path(c->lib, "Statics.dat", path, sizeof path) != 0) return;
    if (!(c->static_groups && lj_statics_groups_save(path, c->show->pages, c->show->npages, c->static_groups) == 0)
        && lj_statics_save(path, c->show->pages, c->show->npages) != 0) lj_log("statics: cannot save %s", path);
}

static void menu_action(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    lj_static_slot *st = slot_ptr(s.pop_slot);
    const lj_sequence *seq = c->show ? c->show->current_seq : NULL;
    const char *a = it ? it->action : NULL;
    int rep = rep_of(a), active = lj_statics2_slot_active(s.pop_slot), rc = 0;
    (void)w; (void)ud;
    if (!a || !st) return;
    if (rep >= 0) { lj_lock(); st->replace = (uint8_t)rep; lj_unlock(); }
    else if (!strcmp(a, "acToggle") || !strcmp(a, "acFlash")) { lj_lock(); st->activation = !strcmp(a, "acFlash") ? LJ_ACT_FLASH : LJ_ACT_TOGGLE; lj_unlock(); }
    else if (!strcmp(a, "acClearSlot")) { lj_desktop_run_action(LJ_ACT_STATIC_CLEAR, lj_static_global_index(lj_s2_cur_page, s.pop_slot)); fill_rows(); return; }   /* the app releases, clears and saves */
    else if (!strcmp(a, "acSlotProperties")) { lj_win_open(LJ_WIN_STATIC_PROPS); return; }
    else if (active) { lj_log("statics: release the slot before %s", it->caption); return; }
    else if (!seq || !seq->scenes || c->current_scene >= seq->n) { lj_log("statics: no current scene"); return; }
    else if (!strcmp(a, "acRecordCurrentScene")) { lj_lock(); rc = lj_static_slot_record(st, &seq->scenes[c->current_scene]); lj_unlock(); }
    else if (!strcmp(a, "acLinkToCurrentScene")) {
        if (!seq->number) { lj_log("statics: save the sequence before linking"); return; }
        lj_lock(); rc = lj_static_slot_link(st, seq->number, c->current_scene); lj_unlock();
    } else if (!strcmp(a, "acConvertToScene")) {
        if (st->kind != LJ_STATIC_RECORDED || !st->scene) return;
        lj_lock();
        memcpy(seq->scenes[c->current_scene].val, st->scene->val, sizeof st->scene->val);
        memcpy(seq->scenes[c->current_scene].state, st->scene->state, sizeof st->scene->state);
        lj_unlock();
        lj_ctx_set_modified(LJ_MOD_SEQ, 1);
        return;
    }
    if (rc != 0) lj_log("statics: %s failed", it->caption);
    save_statics();
    fill_rows();
}

/* ---- widget events ---- */
static int button_of_last(void) { const lj_mouse_ev *m = lj_window_last_event(s.win); return m ? m->button : 1; }
static void slot_event(lj_widget *w, int ev, void *ud)
{
    int i = *(int *)ud, b = button_of_last(), ax, ay;
    if (ev == LJ_WEV_MOUSE_DOWN && b == 1) lj_statics2_press(i + 1, 1);
    else if (ev == LJ_WEV_MOUSE_UP && b == 1) lj_statics2_press(i + 1, 0);
    else if (ev == LJ_WEV_MOUSE_UP && b == 2) { lj_widget_abs(w, &ax, &ay); lj_statics2_popup(i + 1, ax + w->w / 2, ay + w->h / 2); }
    else return;
    lj_statics2_form_refresh();
}
static void mode_event(lj_widget *w, int ev, void *ud)
{
    int i = *(int *)ud, ax, ay;
    if (ev != LJ_WEV_MOUSE_UP) return;
    lj_widget_abs(w, &ax, &ay);
    open_menu(button_of_last() == 2 ? "PopActivationMode" : "PopReplaceMode", i + 1, ax, ay + w->h);
}
static void level_event(lj_widget *w, int ev, void *ud)
{
    const lj_mouse_ev *m = lj_window_last_event(s.win);
    lj_static_slot *st;
    int i = *(int *)ud, ax, ay;
    if (ev == LJ_WEV_MOUSE_DOWN && m && m->button == 1) s.drag = 1;
    else if (ev == LJ_WEV_MOUSE_UP) s.drag = 0;
    else if (ev == LJ_WEV_MOUSE_WHEEL && m && (st = slot_ptr(i + 1)) != NULL) { lj_statics2_set_level(i + 1, st->level_pct + (m->wheel > 0 ? 1 : -1)); return; }
    if (!s.drag || !m || (ev != LJ_WEV_MOUSE_DOWN && ev != LJ_WEV_MOUSE_MOVE)) return;
    lj_widget_abs(w, &ax, &ay);
    lj_statics2_set_level(i + 1, w->w > 1 ? ((m->x - ax) * 100 + (w->w - 1) / 2) / (w->w - 1) : 0);   /* drag across the cell, left = 0 % */
}

static void panel_down(lj_widget *w, void *ud)
{
    (void)ud;
    if (w->name && !strcmp(w->name, "pReleasePage")) lj_statics2_release(lj_s2_cur_page);
    else if (w->name && !strcmp(w->name, "pReleaseAll")) lj_statics2_release(0);
}
static void tb_change(lj_widget *w, void *ud) { (void)ud; lj_statics2_set_page(((lj_w_tabs *)w)->active + 1); }
static void menu_click(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_STATICS2_PREFS); }   /* M_Prefs */
static void group_clicked(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("statics: the group picker is the Statics Preferences Groups tab"); }   /* popGroups' "?" placeholder */
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_statics2_form_close(); }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

static const lj_binding lj_s2_bindings[] = {
    { "PanelMouseDown", panel_down, NULL }, { "PanelMouseMove", noop, NULL }, { "PanelMouseUp", noop, NULL },
    { "tbPagesChange", tb_change, NULL }, { "tbPagesDrawTab", noop, NULL },
    { "acExecute", noop, NULL } /* the TActions fire through their menu items (menu_action) */, { "MenuClick", menu_click, NULL },
    { "popPopup", noop, NULL }, { "popGroupClicked", group_clicked, NULL },
    { "FormActivate", noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL },
    { NULL, NULL, NULL } };

static lj_widget *mk_cell(lj_widget *parent, int y, uint32_t rgb, void (*ev)(lj_widget *, int, void *), int *ud)
{
    lj_widget *b = lj_w_slotbtn_create(parent, NULL);
    b->x = 0; b->y = y; b->w = parent->w; b->h = LJ_S2_ROW_H;
    b->color = rgb; b->on_event = ev; b->ud = ud;
    return b;
}

lj_window *lj_statics2_form_open(void)
{
    lj_widget *pnum, *pst, *pmode, *plvl;
    lj_menu *m;
    int i;
    if (s.win) return s.win;
    memset(&s, 0, sizeof s);
    if (show() && npages() == 0) {                              /* a library without Statics.dat: one "Page 1" (14 sB.3 default) */
        lj_lock();
        if (lj_statics_add_page(&show()->pages, &show()->npages, "Page 1") == 0)
            for (i = 0; i < LJ_S2_N; i++) show()->pages[0].slot[i].level_pct = LJ_STATIC_LEVEL_DEFAULT_PCT;   /* [STATICS] DefaultLevel=255 */
        lj_unlock();
    }
    if (lj_s2_cur_page > npages()) lj_s2_cur_page = 1;
    s.win = lj_window_create(&lj_form_TSTATICS2FORM, lj_s2_bindings);
    if (!s.win) { lj_log("statics: cannot create TStatics2Form"); return NULL; }
    if ((m = lj_window_menus(s.win)) != NULL)
        for (i = 0; i < m->nrt; i++) if (m->rt[i].item->action && !m->rt[i].item->on_click) { m->rt[i].fn = menu_action; m->rt[i].ud = NULL; }   /* Action = acX items: the TAction's OnExecute */
    s.n = lj_statics2_visible();
    lj_window_resize(s.win, lj_form_TSTATICS2FORM.client_width, 58 + s.n * LJ_S2_ROW_H + 30);   /* pPages + n rows + pBottom; PROPOSAL rig 14 sB.8 item 12 (LJ2 sizes it at runtime) */
    pnum = lj_window_find(s.win, "pNumber"); pst = lj_window_find(s.win, "pStatic");
    pmode = lj_window_find(s.win, "pMode"); plvl = lj_window_find(s.win, "pLvl");
    if (!pnum || !pst || !pmode || !plvl) { lj_statics2_form_close(); return NULL; }
    for (i = 0; i < s.n; i++) {
        int y = i * LJ_S2_ROW_H;
        s.idx[i] = i;
        snprintf(s.numtxt[i], sizeof s.numtxt[i], "%d", i + 1);
        s.num[i] = lj_w_label_create(pnum, &lj_s2_label_desc);
        s.num[i]->x = 0; s.num[i]->y = y + 1; s.num[i]->w = pnum->w - 3; s.num[i]->h = LJ_S2_ROW_H - 2;
        lj_w_set_caption(s.num[i], s.numtxt[i]);
        s.btn[i] = mk_cell(pst, y, LJ_CLR_BTNFACE, slot_event, &s.idx[i]);
        s.mode[i] = mk_cell(pmode, y, LJ_CLR_BTNFACE, mode_event, &s.idx[i]);
        s.lvl[i] = mk_cell(plvl, y, LJ_S2_LEVEL_RGB, level_event, &s.idx[i]);
    }
    fill_tabs();
    fill_rows();
    lj_statics2_form_refresh();
    lj_win_show(s.win, LJ_S2_STAY_ON_TOP);
    return s.win;
}

void lj_statics2_form_close(void)
{
    if (!s.win) return;
    lj_win_hide(s.win);
    lj_window_destroy(s.win);
    memset(&s, 0, sizeof s);
}

/* ---- hotkeys (io/lj_actions.h Statics rows): Static NN = slot NN of the current page (PROPOSAL rig 14 sB.8 item 9) ---- */
static void fn_statics(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (fn >= LJ_FN_STATIC_1 && fn <= LJ_FN_STATIC_10) { lj_statics2_press((int)(fn - LJ_FN_STATIC_1) + 1, edge); lj_statics2_form_refresh(); return; }
    if (fn >= LJ_FN_STATIC_11 && fn <= LJ_FN_STATIC_20) { lj_statics2_press((int)(fn - LJ_FN_STATIC_11) + 11, edge); lj_statics2_form_refresh(); return; }
    if (!edge) return;
    if (fn == LJ_FN_STATICS_CONTROL) lj_win_toggle(LJ_WIN_STATICS2_FORM);
    else if (fn == LJ_FN_RELEASE_STATICS) lj_statics2_release(0);
}

void lj_statics2_form_bind_fns(void)
{
    int f;
    lj_fn_bind(LJ_FN_STATICS_CONTROL, fn_statics, NULL);
    lj_fn_bind(LJ_FN_RELEASE_STATICS, fn_statics, NULL);
    for (f = LJ_FN_STATIC_1; f <= LJ_FN_STATIC_10; f++) lj_fn_bind((enum lj_fn)f, fn_statics, NULL);
    for (f = LJ_FN_STATIC_11; f <= LJ_FN_STATIC_20; f++) lj_fn_bind((enum lj_fn)f, fn_statics, NULL);
}
