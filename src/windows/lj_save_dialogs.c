/* lj_save_dialogs.c - behaviour of TSaveSeqForm (lj_save_dialogs.h). */
#include "windows/lj_save_dialogs.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_lists.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_warnings.h"

#define LJ_SS_SEC "SAVESEQFORM"            /* PROPOSAL: our Main.Ini section for this dialog's options */

static lj_window *lj_ss_win;
static int lj_ss_loading, lj_ss_close_pending;
static const char *const lj_ss_types[] = { "<None>" };                            /* [13 exe strings 0x18A2FC]; sequence types are "reserved for future use" [03 s4.2] */
static const char *const lj_ss_slots[] = { "Default", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" };

static lj_sequence *lj_ss_seq(void) { lj_ctx *c = lj_ctx_get(); return c->show ? c->show->current_seq : NULL; }

static int lj_ss_pref(const char *key, int dflt)
{
    lj_ctx *c = lj_ctx_get();
    return c->main_ini ? lj_ini_geti(c->main_ini, LJ_SS_SEC, key, dflt) : dflt;
}

static void lj_ss_pref_set(const char *key, int v)
{
    lj_ctx *c = lj_ctx_get();
    if (c->main_ini) lj_ini_seti(c->main_ini, LJ_SS_SEC, key, v);
}

static void lj_ss_check(const char *name, int on)
{
    lj_widget *w = lj_ss_win ? lj_window_find(lj_ss_win, name) : NULL;
    if (w) lj_w_checkbox_set_checked(w, on);
}

static void lj_ss_update(void)
{
    lj_sequence *s = lj_ss_seq();
    lj_widget *w;
    lj_menu *m;
    char buf[16];
    if (!lj_ss_win) return;
    lj_ss_loading = 1;
    snprintf(buf, sizeof buf, "%04u", (unsigned)(s ? s->number : 0));
    lj_w_edit_set_text(lj_window_find(lj_ss_win, "SeqNum"), s && s->number ? buf : "");   /* blank until the first save [03 s4.2] */
    w = lj_window_find(lj_ss_win, "ESeqName");
    if (w && !w->focused) lj_w_edit_set_text(w, s ? s->name : "");   /* FormActivate re-runs this: a name being typed is not overwritten */
    w = lj_window_find(lj_ss_win, "cbSeqType");
    if (w) { ((lj_w_combobox *)w)->index = 0; lj_widget_invalidate(w); }
    w = lj_window_find(lj_ss_win, "cbSlot");
    if (w) { ((lj_w_combobox *)w)->index = lj_ss_pref("Slot", 0) % (int)LJ_COUNT_OF(lj_ss_slots); lj_widget_invalidate(w); }
    w = lj_window_find(lj_ss_win, "Bsave");
    if (w) { w->enabled = s && s->number; lj_widget_invalidate(w); }
    w = lj_window_find(lj_ss_win, "Panel2");
    if (w) { w->visible = lj_ss_pref("ShowSlot", 0) != 0; lj_window_invalidate_all(lj_ss_win); }
    lj_ss_check("cbClear", lj_ss_pref("Clear", 0));
    lj_ss_check("cbAddSeq", lj_ss_pref("AddToCue", 0));
    lj_ss_check("cbNewCue", lj_ss_pref("NewCue", 0));
    lj_ss_check("cbAutosave", lj_ss_pref("AutoSave", 0));
    m = lj_window_menus(lj_ss_win);
    lj_menu_set_checked(m, "AutoClose", lj_ss_pref("AutoClose", 1));
    lj_menu_set_checked(m, "EnterSave", lj_ss_pref("EnterSave", 0));
    lj_menu_set_checked(m, "M_QuickSaveOptions", lj_ctx_get()->main_ini ? lj_ini_geti(lj_ctx_get()->main_ini, "SEQQUICK", "UseOptions", 1) : 1);
    lj_menu_set_checked(m, "M_SavetoSlot", lj_ss_pref("ShowSlot", 0));
    lj_ss_loading = 0;
}

/* ---- the save ---- */

static void lj_ss_sync_table(const lj_sequence *src)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *rec = (lj_sequence *)lj_table_get(&c->show->sequences, src->number);
    int fresh = rec == NULL;
    if (rec == src) return;
    if (fresh) rec = lj_sequence_new_rec();  /* first save: the table holds a copy of what is on disk; the current record stays the editing copy */
    lj_lock();                               /* a cue pinned this number: the runner keeps its pointer and sees the new content (2.3 rule 6) */
    if (fresh) lj_table_put(&c->show->sequences, src->number, rec);
    if (lj_sequence_alloc(rec, src->n) == 0 && src->scenes) memcpy(rec->scenes, src->scenes, (size_t)src->n * sizeof *src->scenes);
    lj_str_copy(rec->name, sizeof rec->name, src->name);
    lj_str_copy(rec->type, sizeof rec->type, src->type);
    rec->sync_times = src->sync_times; rec->global_scene_time = src->global_scene_time; rec->global_fade_time = src->global_fade_time;
    rec->modified = 0;
    lj_unlock();
    lj_cue_form_repin();                     /* slots holding this number (load error until now) pin the record */
}

static void lj_ss_take_fields(void)
{
    lj_sequence *s = lj_ss_seq();
    const char *name = lj_ss_win ? lj_w_edit_text(lj_window_find(lj_ss_win, "ESeqName")) : NULL;
    if (!s || !name) return;
    lj_lock();
    lj_str_copy(s->name, sizeof s->name, name);
    s->type[0] = 0;                          /* <None>: the only type we list */
    lj_unlock();
}

static int lj_ss_commit(uint32_t number, int use_options)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_ss_seq();
    char path[LJ_PATH_CAP];
    const char *v;
    uint32_t old;
    if (!s || !c->lib || lj_library_record_path(c->lib, LJ_REC_SEQ, number, path, sizeof path) != 0) return -1;
    lj_lock();
    old = s->number; s->number = number;
    lj_unlock();
    v = lj_sequence_validate(s);
    if (v || lj_sequence_save(path, s) != 0) {
        lj_lock(); s->number = old; lj_unlock();
        lj_log("save_seq: %s: %s", path, v ? v : "write failed");
        lj_win_message("Save Sequence", v ? v : "Error writing sequence file");
        return -1;
    }
    s->modified = 0;
    lj_ctx_set_modified(LJ_MOD_SEQ, 0);
    c->seq_saved = 1;                        /* the toolbar number field turns yellow (acceptance 4) */
    lj_ss_sync_table(s);
    lj_seqlist_rebuild();
    lj_seq_form_update();
    if (use_options) {                       /* [03 s4.2] save options, through the cue window's record API (windows-b) */
        if (lj_ss_pref("NewCue", 0) && c->show->current_cue) {
            lj_cue_form_new_cue();
            lj_lock(); lj_str_copy(c->show->current_cue->name, sizeof c->show->current_cue->name, s->name); lj_unlock();   /* "The cue will be given the name of the sequence" */
            lj_cue_form_add_sequence(number);
        } else if (lj_ss_pref("AddToCue", 0)) {
            if (lj_cue_form_add_sequence(number) < 0) lj_win_message("Add Sequence to Cue", "No free sequence slots available");   /* ponytail: "Add to Cue in Slot" picks a specific slot; the cue API takes the first free one */
        }
        if (lj_ss_pref("AutoSave", 0) && c->show->current_cue) lj_cue_form_save(c->show->current_cue->number ? 0 : 1, 0);
        if (lj_ss_pref("Clear", 0)) lj_seq_form_new(1);
    }
    if (lj_ss_win && lj_ss_pref("AutoClose", 1)) lj_ss_close_pending = 1;
    lj_ss_update();
    return 0;
}

static void lj_ss_overwrite_yes(int yes, void *ud) { if (yes && lj_ss_seq()) lj_ss_commit(lj_ss_seq()->number, (int)(intptr_t)ud); }

static int lj_ss_save(int as_new, int use_options)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_ss_seq();
    uint32_t n;
    if (!s || !c->lib) { lj_log("save_seq: no current sequence / library"); return -1; }
    if (as_new || s->number == 0) {
        n = lj_library_next_number(c->lib, LJ_REC_SEQ);
        if (!n) { lj_log("save_seq: lj_library_next_number failed"); return -1; }
        return lj_ss_commit(n, use_options);
    }
    if (c->main_ini && lj_ini_geti(c->main_ini, "PREFS", "WarnOWSeq", 1)) {
        lj_win_confirm("Save Sequence", "Overwrite previously saved sequence", NULL, lj_ss_overwrite_yes, (void *)(intptr_t)use_options);   /* [13 exe strings 0x188DF4] */
        return 0;
    }
    return lj_ss_commit(s->number, use_options);
}

int lj_save_seq_do(int as_new)
{
    lj_ss_take_fields();
    return lj_ss_save(as_new, 1);
}

void lj_save_seq_quick(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_ss_seq();
    if (!s) return;
    if (s->number == 0) { lj_win_open(LJ_WIN_SAVE_SEQ); return; }
    lj_ss_save(0, c->main_ini ? lj_ini_geti(c->main_ini, "SEQQUICK", "UseOptions", 1) : 1);
}

/* ---- handlers ---- */

static void lj_ss_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void lj_ss_on_update(lj_widget *w, void *ud) { (void)w; (void)ud; lj_ss_update(); }
static void lj_ss_on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_ss_close_pending = 1; }
static void lj_ss_on_save(lj_widget *w, void *ud) { (void)w; lj_save_seq_do(ud != NULL); }

static void lj_ss_on_options(lj_widget *w, void *ud)
{
    const char *key = NULL;
    (void)ud;
    if (lj_ss_loading || !w || !w->name) return;
    if (strcmp(w->name, "cbClear") == 0) key = "Clear";
    else if (strcmp(w->name, "cbAddSeq") == 0) key = "AddToCue";
    else if (strcmp(w->name, "cbNewCue") == 0) key = "NewCue";
    else if (strcmp(w->name, "cbAutosave") == 0) key = "AutoSave";
    if (key) lj_ss_pref_set(key, lj_w_checkbox_checked(w));
}

static void lj_ss_on_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_ss_win);
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (strcmp(it->name, "AutoClose") == 0) lj_ss_pref_set("AutoClose", !lj_ss_pref("AutoClose", 1));
    else if (strcmp(it->name, "EnterSave") == 0) lj_ss_pref_set("EnterSave", !lj_ss_pref("EnterSave", 0));
    lj_ss_update();
}

static void lj_ss_on_quick_options(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)w; (void)ud;
    if (c->main_ini) lj_ini_seti(c->main_ini, "SEQQUICK", "UseOptions", !lj_ini_geti(c->main_ini, "SEQQUICK", "UseOptions", 1));
    lj_ss_update();
}

static void lj_ss_on_show_slot(lj_widget *w, void *ud) { (void)w; (void)ud; lj_ss_pref_set("ShowSlot", !lj_ss_pref("ShowSlot", 0)); lj_ss_update(); }

/* FormKeyDown (KeyPreview): "Save on [Enter]" needs the key of the event; lj_window has no
 * last-event accessor yet (W0-last-event gate, integrator), so the pref persists and the
 * check mark shows, but Enter does not save until lj_window_last_event lands. */
static void lj_ss_on_keydown(lj_widget *w, void *ud) { (void)w; (void)ud; }

static const lj_binding lj_ss_bindings[] = {
    { "FormActivate", lj_ss_on_update, NULL }, { "FormClose", lj_ss_on_close, NULL }, { "FormCreate", lj_ss_noop, NULL },
    { "FormDestroy", lj_ss_noop, NULL }, { "FormDeactivate", lj_ss_noop, NULL }, { "FormHide", lj_ss_noop, NULL },
    { "FormKeyDown", lj_ss_on_keydown, NULL }, { "FormShow", lj_ss_on_update, NULL },
    { "cbOptionsClick", lj_ss_on_options, NULL }, { "BsaveClick", lj_ss_on_save, NULL }, { "BSaveNewClick", lj_ss_on_save, (void *)1 },
    { "PrefsClick", lj_ss_on_prefs, NULL }, { "M_QuickSaveOptionsClick", lj_ss_on_quick_options, NULL }, { "M_SavetoSlotClick", lj_ss_on_show_slot, NULL },
    { "Preferences1Click", lj_ss_noop, NULL }, { "Help1Click", lj_ss_noop, NULL },
    { NULL, NULL, NULL }
};

/* ---- open / close / refresh ---- */

lj_window *lj_save_seq_open(void)
{
    const lj_win_host *h = lj_win_host_get();
    lj_widget *w;
    if (lj_ss_win) { if (h && h->show) h->show(h->ud, lj_ss_win, 1); lj_ss_update(); return lj_ss_win; }
    lj_ss_win = lj_window_create(&lj_form_TSAVESEQFORM, lj_ss_bindings);
    if (!lj_ss_win) return NULL;
    w = lj_window_find(lj_ss_win, "cbSeqType");
    if (w) lj_w_combobox_set_items(w, lj_ss_types, LJ_COUNT_OF(lj_ss_types));
    w = lj_window_find(lj_ss_win, "cbSlot");
    if (w) lj_w_combobox_set_items(w, lj_ss_slots, LJ_COUNT_OF(lj_ss_slots));
    lj_ss_close_pending = 0;
    lj_ss_update();
    if (h && h->open && h->open(h->ud, lj_ss_win, 0) != 0) { lj_window_destroy(lj_ss_win); lj_ss_win = NULL; return NULL; }
    return lj_ss_win;
}

void lj_save_seq_close(void)
{
    const lj_win_host *h = lj_win_host_get();
    lj_window *w = lj_ss_win;
    lj_ss_win = NULL; lj_ss_close_pending = 0;
    if (!w) return;
    if (h && h->close) h->close(h->ud, w);
    lj_window_destroy(w);
}

lj_window *lj_save_seq_get(void) { return lj_ss_win; }

void lj_save_seq_refresh(void)
{
    if (lj_ss_close_pending) { lj_save_seq_close(); return; }
    lj_ss_update();
}
