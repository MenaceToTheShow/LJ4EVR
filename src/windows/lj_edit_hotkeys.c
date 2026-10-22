/* lj_edit_hotkeys.c - see lj_edit_hotkeys.h. Every OnXxx of TFUNCTIONSFORM is bound (ME-16). The
 * key capture wraps eKey's key handler (a ReadOnly TEdit with OnChange only in the DFM); the app's
 * keyboard path may also call lj_edit_hotkeys_capture while the dialog is open. */
#include "windows/lj_edit_hotkeys.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_delete_files.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"

enum { LJ_EH_ROWS = LJ_FN_COUNT + LJ_FNG_COUNT, LJ_EH_PROGS = 999 /* ponytail: records listed per tab */ };

static const char *const lj_eh_lv[4] = { "lvSeq", "lvCue", "lvCuelist", "lvBGCue" };
static const char *const lj_eh_assign[4] = { "Assign &Sequence", "Assign &Cue", "Assign Cue&List", "Assign &BGCue" };   /* exe 0x1C2AB4 0x1C2AA0 0x1C2AD0 0x1C2AE8 */
static const enum lj_fn lj_eh_prog_fn[4] = { LJ_FN_PROG_SEQ_FLASH, LJ_FN_PROG_CUE, LJ_FN_PROG_CUELIST, LJ_FN_PROG_BGCUE };
static const enum lj_rec_kind lj_eh_kind[4] = { LJ_REC_SEQ, LJ_REC_CUE, LJ_REC_CL, LJ_REC_BG };

static struct {
    lj_window *win; lj_hotkeys work; int setup, modified, close_pending, tab;
    lj_hotkey pend; int have_pend;
    int nf; uint16_t fn_of[LJ_EH_ROWS]; char fline[LJ_EH_ROWS][112]; const char *fitems[LJ_EH_ROWS];
    int np[4]; uint32_t pnum[4][LJ_EH_PROGS]; char pline[4][LJ_EH_PROGS][96]; const char *pitems[4][LJ_EH_PROGS];
} eh;

static lj_widget *find(const char *n) { return eh.win ? lj_window_find(eh.win, n) : NULL; }
static int sel_index(const char *lv) { lj_widget *w = find(lv); return w ? ((lj_w_listview *)w)->index : -1; }
static void select_row(const char *lv, int row) { lj_widget *w = find(lv); if (w) { ((lj_w_listview *)w)->index = row; lj_widget_invalidate(w); } }

static void combo_cols(enum lj_fn fn, uint32_t num, char *scrl, size_t cs, char *combo, size_t cc)
{
    const lj_hotkey *k = lj_hotkeys_find_fn(&eh.work, fn, num);
    scrl[0] = combo[0] = 0;
    if (!k) return;
    lj_hotkeys_combo_text(k, combo, cc);
    if (k->scroll) snprintf(scrl, cs, "X");   /* PROPOSAL rig: hotkeys2.gif's SCRL mark is not readable */
}

static void fill_functions(void)
{
    int g, f, keep = sel_index("lvFunctions");
    char cap[64], scrl[4], combo[32];
    eh.nf = 0;
    for (g = 0; g < LJ_FNG_COUNT; g++) {
        if (g == LJ_FNG_PROGRAMS) continue;                            /* programs live on the tabs */
        eh.fn_of[eh.nf] = LJ_FN_NONE;
        snprintf(eh.fline[eh.nf], sizeof eh.fline[eh.nf], "%s\t\t", lj_fn_group_names[g]);   /* group header row (PROPOSAL rig: the ODPanel's header look) */
        eh.fitems[eh.nf] = eh.fline[eh.nf]; eh.nf++;
        for (f = 1; f < LJ_FN_COUNT; f++) {
            if (lj_fn_table[f].group != g || (lj_fn_table[f].flags & LJ_FNF_EXT)) continue;
            lj_fn_caption((enum lj_fn)f, cap, sizeof cap);
            combo_cols((enum lj_fn)f, 0, scrl, sizeof scrl, combo, sizeof combo);
            snprintf(eh.fline[eh.nf], sizeof eh.fline[eh.nf], "  %s%s\t%s\t%s", lj_fn_is_flash((enum lj_fn)f) ? "*" : "", cap, scrl, combo);   /* "*" = flash [01 s7] */
            eh.fn_of[eh.nf] = (uint16_t)f; eh.fitems[eh.nf] = eh.fline[eh.nf]; eh.nf++;
        }
    }
    if (find("lvFunctions")) lj_w_listview_set_items(find("lvFunctions"), eh.fitems, eh.nf);
    if (keep >= 0 && keep < eh.nf) select_row("lvFunctions", keep);
}

static void fill_progs(int t)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t nums[LJ_EH_PROGS];
    char name[31], scrl[4], combo[32];
    int i, n = c->lib ? lj_library_list_records(c->lib, lj_eh_kind[t], nums, LJ_EH_PROGS) : 0, keep = sel_index(lj_eh_lv[t]);
    eh.np[t] = n > 0 ? n : 0;
    for (i = 0; i < eh.np[t]; i++) {
        if (lj_delete_files_record_name(lj_eh_kind[t], nums[i], name, sizeof name) != 0) name[0] = 0;
        combo_cols(lj_eh_prog_fn[t], nums[i], scrl, sizeof scrl, combo, sizeof combo);
        snprintf(eh.pline[t][i], sizeof eh.pline[t][i], "%04u\t%s\t%s\t%s", (unsigned)nums[i], name, scrl, combo);
        eh.pnum[t][i] = nums[i]; eh.pitems[t][i] = eh.pline[t][i];
    }
    if (find(lj_eh_lv[t])) lj_w_listview_set_items(find(lj_eh_lv[t]), eh.pitems[t], eh.np[t]);
    if (keep >= 0 && keep < eh.np[t]) select_row(lj_eh_lv[t], keep);
}

/* the DFM greys bAssign bClear bAssignProg bClearProg cbScroll bClearHotkey bFind until there is something to act on */
static void enable(const char *n, int on) { lj_widget *w = find(n); if (w && w->enabled != (unsigned)(on != 0)) { w->enabled = (unsigned)(on != 0); lj_widget_invalidate(w); } }
static void gates(void)
{
    int f = sel_index("lvFunctions"), p = sel_index(lj_eh_lv[eh.tab]);
    int fn_row = f >= 0 && f < eh.nf && eh.fn_of[f] != LJ_FN_NONE, prog_row = p >= 0 && p < eh.np[eh.tab];
    enable("bAssign", fn_row && eh.have_pend); enable("bClear", fn_row);
    enable("bAssignProg", prog_row && eh.have_pend); enable("bClearProg", prog_row);
    enable("cbScroll", eh.have_pend); enable("bClearHotkey", eh.have_pend); enable("bFind", eh.have_pend);
}

static void fill_all(void) { int t; fill_functions(); for (t = 0; t < 4; t++) fill_progs(t); gates(); }

static void set_tab(int t)
{
    eh.tab = t;
    lj_w_set_caption(find("bAssignProg"), lj_eh_assign[t]);
    gates();
}

static void show_pending(void)
{
    char combo[32] = "";
    if (eh.have_pend) lj_hotkeys_combo_text(&eh.pend, combo, sizeof combo);
    lj_w_edit_set_text(find("eKey"), combo);
    gates();
}

int lj_edit_hotkeys_capture(int key, int shift, int ctrl, int alt)
{
    if (!eh.win || alt || key <= 0 || key > 0xFE || key == 0x10 || key == 0x11 || key == 0x12) return -1;   /* Alt never (Windows menus); VK_SHIFT / VK_CONTROL / VK_MENU alone are not keys */
    memset(&eh.pend, 0, sizeof eh.pend);
    eh.pend.key = (uint16_t)key; eh.pend.shift = (uint8_t)(shift != 0); eh.pend.ctrl = (uint8_t)(ctrl != 0);
    eh.have_pend = 1;
    show_pending();
    return 0;
}

/* the assigned entry for the captured combo, or NULL */
static const lj_hotkey *combo_owner(void)
{
    int i;
    for (i = 0; i < eh.work.n; i++)
        if (eh.work.e[i].key == eh.pend.key && eh.work.e[i].shift == eh.pend.shift && eh.work.e[i].ctrl == eh.pend.ctrl) return &eh.work.e[i];
    return NULL;
}

static int assign(enum lj_fn fn, uint32_t num)
{
    const lj_hotkey *o;
    lj_hotkey k;
    char combo[32], what[80], text[160];
    if (!eh.win || !eh.have_pend || fn == LJ_FN_NONE) return -1;
    o = combo_owner();
    if (o && (o->fn != fn || o->prog_num != num)) {
        lj_hotkeys_combo_text(&eh.pend, combo, sizeof combo);
        if (o->fn >= LJ_FN_PROG_SEQ_FLASH && o->fn <= LJ_FN_PROG_BGCUE) snprintf(what, sizeof what, "%s %04u", lj_fn_table[o->fn].caption, (unsigned)o->prog_num);
        else lj_fn_caption((enum lj_fn)o->fn, what, sizeof what);
        snprintf(text, sizeof text, LJ_HOTKEY_CONFLICT_FMT, combo, what);
        lj_win_message("Notification !", text);                          /* TShowHotKeyWarningForm's caption */
        return -1;
    }
    lj_hotkeys_clear_fn(&eh.work, fn, num);                              /* one combo per function / program, as the Key Combo column shows */
    k = eh.pend; k.fn = (uint16_t)fn; k.prog_num = num;
    k.scroll = (uint8_t)lj_w_checkbox_checked(find("cbScroll"));
    if (lj_hotkeys_assign(&eh.work, &k) != 0) { lj_log("edit hotkeys: assign failed (%d entries)", eh.work.n); return -1; }
    eh.modified = 1;
    fill_all();
    return 0;
}

int lj_edit_hotkeys_assign_fn(void)
{
    int r = sel_index("lvFunctions");
    return r >= 0 && r < eh.nf ? assign((enum lj_fn)eh.fn_of[r], 0) : -1;
}

int lj_edit_hotkeys_assign_prog(void)
{
    int r = sel_index(lj_eh_lv[eh.tab]);
    return r >= 0 && r < eh.np[eh.tab] ? assign(lj_eh_prog_fn[eh.tab], eh.pnum[eh.tab][r]) : -1;
}

int lj_edit_hotkeys_select_fn(enum lj_fn fn)
{
    int i;
    for (i = 0; i < eh.nf; i++) if (eh.fn_of[i] == fn && fn != LJ_FN_NONE) { select_row("lvFunctions", i); gates(); return 0; }
    return -1;
}

int lj_edit_hotkeys_select_prog(int t, uint32_t number)
{
    int i;
    if (!eh.win || t < 0 || t > 3) return -1;
    lj_w_tabs_select(find("PC1"), t);
    set_tab(t);
    for (i = 0; i < eh.np[t]; i++) if (eh.pnum[t][i] == number) { select_row(lj_eh_lv[t], i); gates(); return 0; }
    return -1;
}

const lj_hotkeys *lj_edit_hotkeys_work(void) { return eh.win ? &eh.work : NULL; }

static int setup_dir(char *out, size_t cap) { lj_ctx *c = lj_ctx_get(); return c->lib && snprintf(out, cap, "%s", c->lib->root) < (int)cap ? 0 : -1; }

int lj_edit_hotkeys_setup(int setup)
{
    char dir[LJ_PATH_CAP], err[96];
    if (!eh.win || setup < 1 || setup > LJ_HOTKEY_SETUPS_MAX) return -1;
    eh.setup = setup;
    if (setup_dir(dir, sizeof dir) != 0 || lj_hotkeys_setup_load(dir, setup, &eh.work, err, sizeof err) != 0) lj_log("edit hotkeys: setup %d: %s", setup, err);
    eh.modified = 0;
    fill_all();
    return 0;
}

static int save_setup(void)
{
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP];
    if (setup_dir(dir, sizeof dir) != 0 || lj_hotkeys_setup_path(dir, eh.setup, path, sizeof path) != 0) return -1;
    return lj_hotkeys_save(path, &eh.work);
}

int lj_edit_hotkeys_accept(void)
{
    lj_ctx *c = lj_ctx_get();
    int rc;
    if (!eh.win) return -1;
    if (c->hotkeys) *c->hotkeys = eh.work;
    rc = save_setup();
    if (rc != 0) lj_log("edit hotkeys: Setup%d.Hot not written", eh.setup);
    eh.modified = 0;
    return rc;
}

/* ---- eKey: the capture ---- */

static lj_widget_vt lj_eh_vt;
static const lj_widget_vt *lj_eh_base;
static int ekey_key(lj_widget *w, const lj_key_ev *k)
{
    if (k->type == LJ_EV_KEY_DOWN) { lj_edit_hotkeys_capture(k->key, k->shift, k->ctrl, k->alt); return 1; }
    return k->type == LJ_EV_CHAR ? 1 : lj_eh_base->key(w, k);   /* the read-only field takes no typing */
}

/* ---- handlers ---- */

static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void on_assign(lj_widget *w, void *ud) { (void)w; (void)ud; lj_edit_hotkeys_assign_fn(); }
static void on_assign_prog(lj_widget *w, void *ud) { (void)w; (void)ud; lj_edit_hotkeys_assign_prog(); }
static void on_clear_fn(lj_widget *w, void *ud)
{
    int r = sel_index("lvFunctions");
    (void)w; (void)ud;
    if (r >= 0 && r < eh.nf && lj_hotkeys_clear_fn(&eh.work, (enum lj_fn)eh.fn_of[r], 0) == 0) { eh.modified = 1; fill_all(); }
}
static void on_clear_prog(lj_widget *w, void *ud)
{
    int r = sel_index(lj_eh_lv[eh.tab]);
    (void)w; (void)ud;
    if (r >= 0 && r < eh.np[eh.tab] && lj_hotkeys_clear_fn(&eh.work, lj_eh_prog_fn[eh.tab], eh.pnum[eh.tab][r]) == 0) { eh.modified = 1; fill_all(); }
}
static void on_clear_hotkey(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (eh.have_pend && lj_hotkeys_clear_key(&eh.work, eh.pend.key, eh.pend.shift, eh.pend.ctrl) == 0) { eh.modified = 1; fill_all(); }
    eh.have_pend = 0; show_pending();
}
static void on_find(lj_widget *w, void *ud)
{
    const lj_hotkey *o = eh.have_pend ? combo_owner() : NULL;
    int t;
    (void)w; (void)ud;
    if (!o) return;
    for (t = 0; t < 4; t++) if (o->fn == lj_eh_prog_fn[t]) { lj_edit_hotkeys_select_prog(t, o->prog_num); return; }
    lj_edit_hotkeys_select_fn((enum lj_fn)o->fn);
}
static void on_tab(lj_widget *w, void *ud) { (void)ud; set_tab(((lj_w_tabs *)w)->active >= 0 && ((lj_w_tabs *)w)->active < 4 ? ((lj_w_tabs *)w)->active : 0); }
static void on_fn_change(lj_widget *w, void *ud)
{
    int r = ((lj_w_listview *)w)->index;
    const lj_hotkey *k = r >= 0 && r < eh.nf && eh.fn_of[r] ? lj_hotkeys_find_fn(&eh.work, (enum lj_fn)eh.fn_of[r], 0) : NULL;
    (void)ud;
    lj_w_checkbox_set_checked(find("cbScroll"), k && k->scroll);
    gates();
}
static void on_prog_change(lj_widget *w, void *ud) { (void)w; (void)ud; gates(); }
static void on_disable_dialogs(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    eh.work.disable_in_dialogs = (uint8_t)!eh.work.disable_in_dialogs; eh.modified = 1;
    lj_menu_set_checked(lj_window_menus(eh.win), "M_DisableDialogs", eh.work.disable_in_dialogs);
}
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; if (save_setup() != 0) lj_win_message("Warning", "Error Writing Hotkey File"); }   /* PROPOSAL text; SaveDialog1 has no platform dialog yet: the current setup's file */
static void on_load(lj_widget *w, void *ud) { (void)w; (void)ud; lj_edit_hotkeys_setup(eh.setup); }
static void on_default(lj_widget *w, void *ud) { (void)w; (void)ud; lj_hotkeys_default(&eh.work); eh.modified = 1; fill_all(); }
static void on_clear_all_yes(int yes, void *ud) { (void)ud; if (yes && eh.win) { eh.work.n = 0; eh.modified = 1; fill_all(); } }
static void on_clear_all(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Please Confirm..", "Clear all assignments, are you sure ?", NULL, on_clear_all_yes, NULL); }   /* exe 0x167258 (owner form UNVERIFIED) */
static void scroll_set(int prog, int on)
{
    int r = prog ? sel_index(lj_eh_lv[eh.tab]) : sel_index("lvFunctions");
    enum lj_fn fn = prog ? (r >= 0 && r < eh.np[eh.tab] ? lj_eh_prog_fn[eh.tab] : LJ_FN_NONE) : (r >= 0 && r < eh.nf ? (enum lj_fn)eh.fn_of[r] : LJ_FN_NONE);
    uint32_t num = prog && r >= 0 && r < eh.np[eh.tab] ? eh.pnum[eh.tab][r] : 0;
    int i;
    for (i = 0; fn != LJ_FN_NONE && i < eh.work.n; i++) if (eh.work.e[i].fn == fn && eh.work.e[i].prog_num == num) { eh.work.e[i].scroll = (uint8_t)on; eh.modified = 1; }
    fill_all();
}
static void on_scroll1(lj_widget *w, void *ud) { const lj_menu_item_desc *it = lj_window_menu_item(eh.win); (void)w; (void)ud; scroll_set(0, !(it && it->name && strncmp(it->name, "Clear", 5) == 0)); }
static void on_scroll2(lj_widget *w, void *ud) { const lj_menu_item_desc *it = lj_window_menu_item(eh.win); (void)w; (void)ud; scroll_set(1, !(it && it->name && strncmp(it->name, "Clear", 5) == 0)); }
static void on_clear1(lj_widget *w, void *ud) { (void)w; (void)ud; on_clear_fn(NULL, NULL); }
static void on_clear2(lj_widget *w, void *ud) { (void)w; (void)ud; on_clear_prog(NULL, NULL); }
static void on_ok(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_edit_hotkeys_accept() == 0) eh.close_pending = 1; }
static void on_discard(int yes, void *ud) { (void)ud; if (yes) eh.close_pending = 1; }
static void on_cancel(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (eh.modified) lj_win_confirm("Please Confirm..", LJ_HOTKEY_CHANGED, NULL, on_discard, NULL);
    else eh.close_pending = 1;
}
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("edit hotkeys: Help is W4"); }

static const lj_binding lj_eh_bindings[] = {
    { "_h", noop, NULL }, { "FormClose", on_cancel, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL },
    { "bAssignProgClick", on_assign_prog, NULL }, { "bClearProgClick", on_clear_prog, NULL }, { "PC1Change", on_tab, NULL },
    { "lvProgChange", on_prog_change, NULL }, { "lvProgClick", on_prog_change, NULL }, { "eKeyChange", noop, NULL },
    { "bClearHotkeyClick", on_clear_hotkey, NULL }, { "bFindClick", on_find, NULL }, { "lvFunctionsChange", on_fn_change, NULL },
    { "bAssignClick", on_assign, NULL }, { "bClearFunctionClick", on_clear_fn, NULL }, { "bOkClick", on_ok, NULL }, { "bCancelClick", on_cancel, NULL },
    { "M_DisableDialogsClick", on_disable_dialogs, NULL }, { "SaveHotkeyLayout1Click", on_save, NULL }, { "LoadHotkeys1Click", on_load, NULL },
    { "DefaultHotkeys1Click", on_default, NULL }, { "ClearAllHotkeys1Click", on_clear_all, NULL }, { "Help1Click", on_help, NULL },
    { "PopupMenu1Popup", noop, NULL }, { "ClearFunctions1Click", on_clear1, NULL }, { "SetScrollLock1Click", on_scroll1, NULL },
    { "PopupMenu2Popup", noop, NULL }, { "ClearFunctions2Click", on_clear2, NULL }, { "SetScrollLock2Click", on_scroll2, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_edit_hotkeys_open(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *ek;
    if (eh.win) return eh.win;
    eh.win = lj_window_create(&lj_form_TFUNCTIONSFORM, lj_eh_bindings);
    if (!eh.win) { lj_log("edit hotkeys: cannot create TFunctionsForm"); return NULL; }
    eh.close_pending = 0; eh.modified = 0; eh.have_pend = 0; eh.tab = 0;
    eh.setup = c->prefs && c->prefs->hotkey_setup >= 1 && c->prefs->hotkey_setup <= LJ_HOTKEY_SETUPS_MAX ? c->prefs->hotkey_setup : 1;
    if (c->hotkeys) eh.work = *c->hotkeys; else lj_hotkeys_init(&eh.work);
    if ((ek = find("eKey")) != NULL) {
        if (!lj_eh_base) { lj_eh_base = ek->vt; lj_eh_vt = *ek->vt; lj_eh_vt.key = ekey_key; }
        ek->vt = &lj_eh_vt;
    }
    lj_menu_set_checked(lj_window_menus(eh.win), "M_DisableDialogs", eh.work.disable_in_dialogs);
    lj_w_tabs_select(find("PC1"), 0);
    set_tab(0);
    fill_all();
    show_pending();
    lj_win_show(eh.win, 0);
    return eh.win;
}

void lj_edit_hotkeys_close(void)
{
    if (!eh.win) return;
    lj_win_hide(eh.win);
    lj_window_destroy(eh.win);
    eh.win = NULL; eh.close_pending = 0;
}

lj_window *lj_edit_hotkeys_window(void) { return eh.win; }
void lj_edit_hotkeys_refresh(void) { if (eh.close_pending) lj_edit_hotkeys_close(); }
