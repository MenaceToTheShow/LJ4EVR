/* lj_bgcue_form.c - the Background Cue window (lj_bgcue_form.h). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_bgcue_form.h"

#include "core/lj_log.h"
#include "core/lj_str.h"
#include "ui/lj_dnd.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "model/lj_bgcue.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_w_slotbtn.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_lists.h"
#include "windows/lj_windows.h"

#define LJ_BG_STAY_ON_TOP 2u            /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here) */
#define LJ_BG_ON_RGB      0x00FF00u     /* bgcuecontrol.gif: the numbered buttons green when the slot runs */
#define LJ_BG_OFF_RGB     0xFF0000u     /* red depressed when off, as the cue control's enable buttons [07 s3.5] */

static const char *const lj_bg_mode_word[] = { "", "A", "-L" };   /* Auto / Audio trigged / No-Loop [03 s7] */
static const char *const lj_bg_nums[LJ_BG_SLOTS] = { "1", "2", "3", "4", "5" };

static struct {
    lj_window *win;
    lj_widget *btn[LJ_BG_SLOTS];
    int idx[LJ_BG_SLOTS];
    char row[LJ_BG_SLOTS][96]; const char *rows[LJ_BG_SLOTS];
    char combo[LJ_BG_FILES][40]; const char *combo_items[LJ_BG_FILES]; uint32_t combo_no[LJ_BG_FILES]; int ncombo;
    uint32_t shown_bg;                  /* the bg_number the combo shows */
    lj_cmd last; int has_last;
} s;

/* The record the window shows and edits (outlives the window, as LJ2's current BG cue): a load copies the pinned record in,
 * a drop / Clear Slot / save edits it and hands it to the app (LJ_ACT_BGCUE_APPLY), which pins a copy (design 2.3 rules 2, 6). */
static lj_bgcue lj_bg_work;
static struct { lj_window *win; } sv;   /* Save Background Cue (TSaveBGForm) */

const lj_bgcue *lj_bgcue_form_work(void) { return &lj_bg_work; }
lj_window *lj_bgcue_form_get(void) { return s.win; }
lj_widget *lj_bgcue_form_slot_button(int slot) { return slot >= 1 && slot <= LJ_BG_SLOTS ? s.btn[slot - 1] : NULL; }
int lj_bgcue_form_last_cmd(lj_cmd *out) { if (out && s.has_last) *out = s.last; return s.has_last; }

static void post_on(uint32_t runner, int on)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_BG_ON; cmd.a = runner; cmd.b = on != 0;
    s.last = cmd; s.has_last = 1;
    lj_ctx_post(&cmd);
}

void lj_bgcue_form_toggle_slot(int slot)
{
    if (slot < 1 || slot > LJ_BG_SLOTS) return;
    post_on((uint32_t)(LJ_RUN_BG0 + slot - 1), !lj_ctx_get()->view.bg_enabled[slot - 1]);
}

/* a BG cue loaded elsewhere (combo, cue latch, hotkey, startup) becomes the edited record */
static void sync_work(void)
{
    lj_ctx *c = lj_ctx_get();
    const lj_bgcue *rec;
    if (!c->show || !c->view.bg_number || c->view.bg_number == lj_bg_work.number) return;
    if ((rec = (const lj_bgcue *)lj_table_get(&c->show->bgcues, c->view.bg_number)) != NULL) lj_bg_work = *rec;
}
static const lj_bgcue *loaded(void) { return &lj_bg_work; }
static void apply(void) { lj_desktop_run_action(LJ_ACT_BGCUE_APPLY, 0); }

int lj_bgcue_form_row(int slot, char *dst, size_t cap)
{
    lj_ctx *c = lj_ctx_get();
    const lj_engine_status *v = &c->view;
    const lj_bgcue *bg = loaded();
    const lj_sequence *seq;
    char no[12], scene[16];
    uint32_t n;
    int i = slot - 1, mode, k;
    if (slot < 1 || slot > LJ_BG_SLOTS || !dst || cap == 0) return -1;
    n = bg ? bg->slot[i].seq : 0;
    if (!n) { dst[0] = 0; return 0; }
    seq = c->show ? (const lj_sequence *)lj_table_get(&c->show->sequences, n) : NULL;
    if (lj_number_text(n, no, sizeof no) != 0) no[0] = 0;
    scene[0] = 0;
    if (v->bg_status[i] == LJ_SLOT_LOAD_ERROR) snprintf(scene, sizeof scene, "load error");      /* 03 s5.1 wording, as the cue grid */
    else if (v->bg_status[i] != LJ_SLOT_EMPTY) snprintf(scene, sizeof scene, "%u:%u", (unsigned)v->bg_scene[i], (unsigned)v->bg_scenes[i]);
    mode = bg->slot[i].mode <= LJ_BG_NOLOOP ? bg->slot[i].mode : 0;
    k = snprintf(dst, cap, "%s\t%s\t%s\t%s", no, seq ? seq->name : "", scene, lj_bg_mode_word[mode]);
    return k < 0 || (size_t)k >= cap ? -1 : 0;
}

/* cb1: every BG cue file of the library, "01 Wild Bar Lights" (bgcuecontrol.gif) */
static void fill_combo(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *cb = lj_window_find(s.win, "cb1");
    static lj_bgcue tmp;                                      /* one record read at a time for its name */
    char path[LJ_PATH_CAP], err[128];
    int n = c->lib ? lj_library_list_records(c->lib, LJ_REC_BG, s.combo_no, LJ_BG_FILES) : 0, i;
    if (n < 0) n = 0;
    for (i = 0; i < n; i++) {
        const lj_bgcue *rec = c->show ? (const lj_bgcue *)lj_table_get(&c->show->bgcues, s.combo_no[i]) : NULL;
        const char *name = rec ? rec->name : "";
        if (!rec && lj_library_record_path(c->lib, LJ_REC_BG, s.combo_no[i], path, sizeof path) == 0 && lj_bgcue_load(path, &tmp, err, sizeof err) == 0) name = tmp.name;
        snprintf(s.combo[i], sizeof s.combo[i], "%02u %s", (unsigned)s.combo_no[i], name);
        s.combo_items[i] = s.combo[i];
    }
    s.ncombo = n;
    if (cb) lj_w_combobox_set_items(cb, s.combo_items, n);
    s.shown_bg = 0xFFFFFFFFu;                                 /* re-select on the next refresh */
}

void lj_bgcue_form_refresh(void)
{
    const lj_engine_status *v = &lj_ctx_get()->view;
    lj_widget *lv, *cb;
    int i;
    if (v->tick_no) lj_desktop_led_set(LJ_LED_BGCUE, v->bg_any);
    sync_work();
    if (!s.win) return;
    for (i = 0; i < LJ_BG_SLOTS; i++) {
        if (lj_bgcue_form_row(i + 1, s.row[i], sizeof s.row[i]) != 0) s.row[i][0] = 0;
        s.rows[i] = s.row[i];
        if (s.btn[i]) {
            int on = v->bg_enabled[i] || !lj_bg_work.slot[i].seq;      /* bgcuecontrol.gif: an empty slot is green too (every slot defaults on [03 s7]) */
            uint32_t rgb = on ? LJ_BG_ON_RGB : LJ_BG_OFF_RGB;
            if (s.btn[i]->color != rgb) s.btn[i]->color = rgb;
            lj_w_slotbtn_set(s.btn[i], lj_bg_nums[i], !on);
        }
    }
    if ((lv = lj_window_find(s.win, "LV1")) != NULL) {
        lj_w_listview *l = (lj_w_listview *)lv;
        int keep = l->index;
        lj_w_listview_set_items(lv, s.rows, LJ_BG_SLOTS);
        l->index = keep;
    }
    if (s.shown_bg != v->bg_number && (cb = lj_window_find(s.win, "cb1")) != NULL) {
        lj_w_combobox *c = (lj_w_combobox *)cb;
        c->index = -1;
        for (i = 0; i < s.ncombo; i++) if (s.combo_no[i] == v->bg_number) c->index = i;
        s.shown_bg = v->bg_number;
        lj_widget_invalidate(cb);
    }
}

/* ---- events ---- */
static void slot_event(lj_widget *w, int ev, void *ud)
{
    (void)w;
    if (ev == LJ_WEV_MOUSE_DOWN && lj_window_last_event(s.win)->button == 1) lj_bgcue_form_toggle_slot(*(int *)ud + 1);
}
static void cb_change(lj_widget *w, void *ud)
{
    int i = ((lj_w_combobox *)w)->index;
    (void)ud;
    if (i < 0 || i >= s.ncombo) return;
    s.shown_bg = s.combo_no[i];
    lj_desktop_run_action(LJ_ACT_LOAD_BGCUE, (int)s.combo_no[i]);
}
static void sb_new(lj_widget *w, void *ud) { (void)w; (void)ud; memset(&lj_bg_work, 0, sizeof lj_bg_work); lj_desktop_run_action(LJ_ACT_LOAD_BGCUE, 0); }
static void sb_delete(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_DELETE_BG); }
static void sb_load(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_SELECT_BGCUE); }
static void sb_save(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lj_win_open(LJ_WIN_SAVE_BGCUE); }   /* sbSave has no DFM event: LJ2 wires it in code */

/* Main.Ini [BGCUE] ConfirmClear / ConfirmDelete / RestoreState: our keys (PROPOSAL rig: LJ2's are not in the shipped file);
 * RestoreState is read by the app's load (LJ_BG_RESTORE_ONOFF) */
static const char *const lj_bg_pref_items[] = { "M_ConfirmClear", "M_ConfirmDelete", "M_RestoreState" };
static const char *const lj_bg_pref_keys[] = { "ConfirmClear", "ConfirmDelete", "RestoreState" };
static void sync_checks(void)
{
    lj_ctx *c = lj_ctx_get();
    int i;
    for (i = 0; i < (int)LJ_COUNT_OF(lj_bg_pref_keys); i++)
        lj_menu_set_checked(lj_window_menus(s.win), lj_bg_pref_items[i], c->main_ini ? lj_ini_geti(c->main_ini, "BGCUE", lj_bg_pref_keys[i], i == 2 ? LJ_BG_RESTORE_ONOFF : 1) : 0);
}
static void pref_click(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    int i;
    (void)w; (void)ud;
    for (i = 0; it && it->name && c->main_ini && i < (int)LJ_COUNT_OF(lj_bg_pref_keys); i++)
        if (!strcmp(it->name, lj_bg_pref_items[i])) lj_ini_seti(c->main_ini, "BGCUE", lj_bg_pref_keys[i], !lj_ini_geti(c->main_ini, "BGCUE", lj_bg_pref_keys[i], i == 2 ? LJ_BG_RESTORE_ONOFF : 1));
    sync_checks();
}
/* ponytail: Sequence Mode.. (TBGCueSequenceModeForm) and Edit Sequence are not wired yet; logged */
static void later(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("bgcue form: Sequence Mode.. / Edit Sequence not implemented yet"); }
/* a sequence dragged from the List of Sequences onto a row fills that slot, past the rows the first free one [03 s7] */
static void lv_drop(lj_widget *w, void *ud)
{
    int kind, number, x, y, row;
    (void)ud;
    if (lj_dnd_info(s.win, &kind, &number, &x, &y) != 0 || kind != LJ_DND_SEQUENCE || number <= 0) return;   /* the target window holds the drop point */
    row = lj_w_listview_row_at(w, y);
    if (row < 0 || row >= LJ_BG_SLOTS) lj_bgcue_add_sequence(&lj_bg_work, (uint32_t)number);
    else { lj_bg_work.slot[row].seq = (uint32_t)number; lj_bg_work.slot[row].enabled = 1; lj_bg_work.slot[row].mode = LJ_BG_AUTO; }
    apply();
}
static void clear_slot(lj_widget *w, void *ud)
{
    lj_widget *lv = lj_window_find(s.win, "LV1");
    int i = lv ? ((lj_w_listview *)lv)->index : -1;
    (void)w; (void)ud;
    if (i < 0 || i >= LJ_BG_SLOTS || !lj_bg_work.slot[i].seq) return;
    memset(&lj_bg_work.slot[i], 0, sizeof lj_bg_work.slot[i]);
    apply();
}
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_bgcue_form_close(); }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_bgcue_form_refresh(); }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

static const lj_binding lj_bg_bindings[] = {
    { "sbNewClick", sb_new, NULL }, { "sbDeleteClick", sb_delete, NULL }, { "sbMouseUp", sb_load, NULL }, { "cb1Change", cb_change, NULL },
    { "LV1DblClick", noop, NULL }, { "LV1DragDrop", lv_drop, NULL }, { "LV1DragOver", noop, NULL },
    { "BGCueTimerTimer", on_timer, NULL }, { "PopupMenu1Popup", noop, NULL },
    { "P_SequenceModeClick", later, NULL }, { "P_EditSequenceClick", later, NULL }, { "P_ClearSlotClick", clear_slot, NULL },
    { "PrefClicks", pref_click, NULL }, { "Help1Click", noop, NULL },
    { "FormClose", on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormShow", noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_bgcue_form_open(void)
{
    lj_widget *w;
    int i;
    if (s.win) return s.win;
    memset(&s, 0, sizeof s);
    s.win = lj_window_create(&lj_form_TBGCUEFORM, lj_bg_bindings);
    if (!s.win) { lj_log("bgcue form: cannot create TBGCueForm"); return NULL; }
    for (i = 0; i < LJ_BG_SLOTS; i++) {
        char nm[4];
        lj_widget *sb;
        snprintf(nm, sizeof nm, "sb%d", i + 1);
        if ((sb = lj_window_find(s.win, nm)) == NULL) { lj_bgcue_form_close(); return NULL; }
        sb->visible = 0;                                           /* the DFM's 30x14 speed buttons carry no art: the numbered buttons are drawn over their rects */
        s.idx[i] = i;
        s.btn[i] = lj_w_slotbtn_create(sb->parent, NULL);
        s.btn[i]->x = sb->x; s.btn[i]->y = sb->y; s.btn[i]->w = sb->w; s.btn[i]->h = sb->h;
        s.btn[i]->on_event = slot_event; s.btn[i]->ud = &s.idx[i];
    }
    if ((w = lj_window_find(s.win, "sbSave")) != NULL) { w->on_event = sb_save; w->ud = NULL; }
    fill_combo();
    sync_checks();
    lj_bgcue_form_refresh();
    lj_win_show(s.win, LJ_BG_STAY_ON_TOP);
    return s.win;
}

void lj_bgcue_form_close(void)
{
    if (!s.win) return;
    lj_win_hide(s.win);
    lj_window_destroy(s.win);
    memset(&s, 0, sizeof s);
}

/* ---- Save Background Cue (TSaveBGForm 198x92 [03 s7]): # and Name, Save (the record's number, greyed while it has none) /
 * Save New (the next [NEXT] number), "Save as Transparent BGCue"; Auto Close in forms.ini [SAVEBGFORM] (PROPOSAL key) ---- */
static int sv_geti(const char *key, int dflt) { lj_ctx *c = lj_ctx_get(); return c->forms_ini ? lj_ini_geti(c->forms_ini, "SAVEBGFORM", key, dflt) : dflt; }
static void sv_fill(void)
{
    char no[12];
    lj_widget *w;
    if (!sv.win) return;
    if (!lj_bg_work.number || lj_number_text(lj_bg_work.number, no, sizeof no) != 0) no[0] = 0;
    if ((w = lj_window_find(sv.win, "ENo")) != NULL) lj_w_edit_set_text(w, no);
    if ((w = lj_window_find(sv.win, "eName")) != NULL) lj_w_edit_set_text(w, lj_bg_work.name);
    if ((w = lj_window_find(sv.win, "cbTransparent")) != NULL) lj_w_checkbox_set_checked(w, lj_bg_work.transparent);
    if ((w = lj_window_find(sv.win, "Bsave")) != NULL) { w->enabled = lj_bg_work.number != 0; lj_widget_invalidate(w); }
    lj_menu_set_checked(lj_window_menus(sv.win), "M_AutoClose", sv_geti("AutoClose", 1));
    lj_menu_set_checked(lj_window_menus(sv.win), "M_EnterSave", sv_geti("SaveOnEnter", 0));
}
static void sv_save(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    lj_bgcue rec = lj_bg_work;
    char path[LJ_PATH_CAP];
    const char *bad;
    lj_widget *e;
    (void)w;
    if (!c->lib) return;
    if ((e = lj_window_find(sv.win, "eName")) != NULL) lj_str_copy(rec.name, sizeof rec.name, lj_w_edit_text(e));
    if ((e = lj_window_find(sv.win, "cbTransparent")) != NULL) rec.transparent = (uint8_t)lj_w_checkbox_checked(e);
    if (ud || !rec.number) rec.number = lj_library_next_number(c->lib, LJ_REC_BG);
    if (!rec.number) { lj_log("save bg cue: no number"); return; }
    if ((bad = lj_bgcue_validate(&rec)) != NULL) { lj_log("save bg cue: %s", bad); return; }
    if (lj_library_record_path(c->lib, LJ_REC_BG, rec.number, path, sizeof path) != 0 || lj_bgcue_save(path, &rec) != 0) { lj_log("save bg cue: %04u not written", (unsigned)rec.number); return; }
    lj_bg_work = rec;
    apply();
    if (s.win) fill_combo();
    if (sv_geti("AutoClose", 1)) lj_save_bg_close(); else sv_fill();
}
static void sv_prefs(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const lj_menu_item_desc *it = lj_window_menu_item(sv.win);
    (void)w; (void)ud;
    if (!it || !it->name || !c->forms_ini) return;
    if (!strcmp(it->name, "M_AutoClose")) lj_ini_seti(c->forms_ini, "SAVEBGFORM", "AutoClose", !sv_geti("AutoClose", 1));
    else if (!strcmp(it->name, "M_EnterSave")) lj_ini_seti(c->forms_ini, "SAVEBGFORM", "SaveOnEnter", !sv_geti("SaveOnEnter", 0));
    sv_fill();
}
static const lj_binding sv_bindings[] = {
    { "BsaveClick", sv_save, NULL }, { "BSaveNewClick", sv_save, (void *)1 }, { "PrefsClick", sv_prefs, NULL }, { "Help1Click", noop, NULL },
    { "FormClose", noop, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormKeyDown", noop, NULL }, { "FormShow", noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_save_bg_open(void)
{
    if (sv.win) return sv.win;
    if ((sv.win = lj_window_create(&lj_form_TSAVEBGFORM, sv_bindings)) == NULL) { lj_log("bgcue form: cannot create TSaveBGForm"); return NULL; }
    sv_fill();
    lj_win_show(sv.win, LJ_BG_STAY_ON_TOP);
    return sv.win;
}
void lj_save_bg_close(void)
{
    if (!sv.win) return;
    lj_win_hide(sv.win);
    lj_window_destroy(sv.win);
    sv.win = NULL;
}
lj_window *lj_save_bg_get(void) { return sv.win; }

/* ---- hotkeys (io/lj_actions.h BG cue rows) ---- */
static void fn_bg(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (!edge) return;
    if (fn >= LJ_FN_BGCUE_TOGGLE_SLOT_1 && fn <= LJ_FN_BGCUE_TOGGLE_SLOT_5) { lj_bgcue_form_toggle_slot((int)(fn - LJ_FN_BGCUE_TOGGLE_SLOT_1) + 1); return; }
    switch (fn) {
    case LJ_FN_TOGGLE_BGCUE_CONTROL: lj_win_toggle(LJ_WIN_BGCUE_FORM); break;
    case LJ_FN_TOGGLE_BG_CUE_LIST:   lj_win_toggle(LJ_WIN_SELECT_BGCUE); break;
    case LJ_FN_CLEAR_BGCUE:          memset(&lj_bg_work, 0, sizeof lj_bg_work); lj_desktop_run_action(LJ_ACT_LOAD_BGCUE, 0); break;
    case LJ_FN_BGCUE_ALL_SLOTS_OFF:  post_on(LJ_RUN_ALL, 0); break;
    case LJ_FN_BGCUE_ALL_SLOTS_ON:   post_on(LJ_RUN_ALL, 1); break;
    default: break;
    }
}

void lj_bgcue_form_bind_fns(void)
{
    static const enum lj_fn rows[] = { LJ_FN_TOGGLE_BGCUE_CONTROL, LJ_FN_TOGGLE_BG_CUE_LIST, LJ_FN_CLEAR_BGCUE, LJ_FN_BGCUE_ALL_SLOTS_OFF, LJ_FN_BGCUE_ALL_SLOTS_ON,
                                       LJ_FN_BGCUE_TOGGLE_SLOT_1, LJ_FN_BGCUE_TOGGLE_SLOT_2, LJ_FN_BGCUE_TOGGLE_SLOT_3, LJ_FN_BGCUE_TOGGLE_SLOT_4, LJ_FN_BGCUE_TOGGLE_SLOT_5 };
    int i;
    for (i = 0; i < (int)LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], fn_bg, NULL);
}
