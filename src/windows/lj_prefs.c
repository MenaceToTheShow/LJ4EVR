/* lj_prefs.c - see lj_prefs.h. Every OnXxx of TSTARTUPFORM is bound (ME-16). W4 (dlg-desktop): the number fields are
 * read back on Ok, Select Startup Cuelist opens TSelectCuelist2ModalForm (its pick comes back through
 * lj_startup_options_set_cl), the other Select buttons open their lists (the number is typed), and the Auto Launch list
 * lives in Startup.ini [AUTOLAUNCH] Count File1..n (ours, PROPOSAL: LJ2 keeps it in the install folder's startup.ini). */
#include "windows/lj_prefs.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "windows/lj_desktop_dialogs.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_show_dialogs.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"

typedef struct { lj_window *win; lj_startup_opts o; char launch[LJ_LAUNCH_MAX][LJ_PATH_CAP]; const char *launch_ptr[LJ_LAUNCH_MAX]; int nlaunch; } lj_so_state;
static lj_so_state so;

void lj_startup_opts_load(lj_ini *ini, lj_startup_opts *o)
{
    memset(o, 0, sizeof *o);
    o->seq = o->cue = o->cl = o->pcue = -1;
    if (!ini) return;
    o->auto_lamp_on = lj_ini_geti(ini, "STARTUP", "AutoLampOn", 0);
    o->seq = lj_ini_geti(ini, "STARTUP", "Seq", -1); o->cue = lj_ini_geti(ini, "STARTUP", "Cue", -1);
    o->cl = lj_ini_geti(ini, "STARTUP", "Cl", -1); o->pcue = lj_ini_geti(ini, "STARTUP", "PCUE", -1);
    o->seq_run = (uint8_t)(lj_ini_geti(ini, "STARTUP", "SeqRun", 0) != 0); o->cl_run = (uint8_t)(lj_ini_geti(ini, "STARTUP", "ClRun", 0) != 0);
    o->minimized = (uint8_t)(lj_ini_geti(ini, "STARTUP", "Minimized", 0) != 0); o->show_notes = (uint8_t)(lj_ini_geti(ini, "STARTUP", "ShowNotes", 0) != 0);
    if (o->auto_lamp_on < 0 || o->auto_lamp_on > 5) o->auto_lamp_on = 0;
}

void lj_startup_opts_save(lj_ini *ini, const lj_startup_opts *o)
{
    if (!ini) return;
    lj_ini_seti(ini, "STARTUP", "AutoLampOn", o->auto_lamp_on);
    lj_ini_seti(ini, "STARTUP", "Seq", o->seq); lj_ini_seti(ini, "STARTUP", "Cue", o->cue);
    lj_ini_seti(ini, "STARTUP", "Cl", o->cl); lj_ini_seti(ini, "STARTUP", "PCUE", o->pcue);
    lj_ini_seti(ini, "STARTUP", "SeqRun", o->seq_run); lj_ini_seti(ini, "STARTUP", "ClRun", o->cl_run);
    lj_ini_seti(ini, "STARTUP", "Minimized", o->minimized); lj_ini_seti(ini, "STARTUP", "ShowNotes", o->show_notes);
}

lj_window *lj_startup_options_window(void) { return so.win; }
static lj_widget *find(const char *n) { return lj_window_find(so.win, n); }
static void number_field(const char *name, int32_t v) { char t[16]; if (v >= 0) snprintf(t, sizeof t, "%04d", (int)v); else t[0] = 0; lj_w_edit_set_text(find(name), t); }   /* 4-digit display [F28] */

static void page_fill(void)
{
    number_field("ESeqNum", so.o.seq); number_field("ECueNum", so.o.cue); number_field("EClNum", so.o.cl); number_field("ePCueNum", so.o.pcue);
    lj_w_checkbox_set_checked(find("SeqRun"), so.o.seq_run); lj_w_checkbox_set_checked(find("ClRun"), so.o.cl_run);
    lj_w_checkbox_set_checked(find("cbRunMinimized"), so.o.minimized); lj_w_checkbox_set_checked(find("cbShowNotes"), so.o.show_notes);
    lj_w_radiogroup_set_index(find("RGAutolamp"), so.o.auto_lamp_on);
    { int i; for (i = 0; i < so.nlaunch; i++) so.launch_ptr[i] = so.launch[i]; lj_w_list_set_items(find("lbauto"), so.launch_ptr, so.nlaunch); }
}

static int32_t number_back(const char *name)   /* the typed number, -1 when blank or not a number */
{
    const char *t = lj_w_edit_text(find(name));
    char *end;
    long v;
    if (!t || !*t) return -1;
    v = strtol(t, &end, 10);
    return *end || v < 0 || v > 99999999 ? -1 : (int32_t)v;
}

int lj_startup_options_apply(void)
{
    lj_ctx *ctx = lj_ctx_get();
    if (!so.win) return -1;
    so.o.seq_run = (uint8_t)lj_w_checkbox_checked(find("SeqRun")); so.o.cl_run = (uint8_t)lj_w_checkbox_checked(find("ClRun"));
    so.o.minimized = (uint8_t)lj_w_checkbox_checked(find("cbRunMinimized")); so.o.show_notes = (uint8_t)lj_w_checkbox_checked(find("cbShowNotes"));
    so.o.auto_lamp_on = lj_w_radiogroup_index(find("RGAutolamp")); if (so.o.auto_lamp_on < 0) so.o.auto_lamp_on = 0;
    so.o.seq = number_back("ESeqNum"); so.o.cue = number_back("ECueNum"); so.o.cl = number_back("EClNum"); so.o.pcue = number_back("ePCueNum");
    lj_startup_opts_save(ctx->main_ini, &so.o);
    lj_auto_launch_save((const char *const *)so.launch_ptr, so.nlaunch);
    return 0;
}

static void on_select(lj_widget *w, void *ud)   /* ponytail: the Sequence / Cue / BG lists have no pick-back seam; the number is typed into the field */
{
    (void)w;
    lj_win_open((enum lj_win)(intptr_t)ud);
}
static void on_browse(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_dlg_pick(LJ_PICK_LAUNCH); }
static void on_delete(lj_widget *w, void *ud)   /* AutoPop Delete Entry: the selected row */
{
    const lj_w_list *l = (const lj_w_list *)find("lbauto");
    (void)w; (void)ud;
    if (!l || l->index < 0 || l->index >= so.nlaunch) return;
    memmove(so.launch[l->index], so.launch[l->index + 1], sizeof so.launch[0] * (size_t)(so.nlaunch - l->index - 1));
    so.nlaunch--;
    page_fill();
}
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_help_viewer_topic(NULL); }
static void on_ok(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_startup_options_apply() == 0) lj_startup_options_close(); }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_startup_options_close(); }
static void on_clear(lj_widget *w, void *ud)
{
    (void)ud;
    if (!w->name) return;
    if (strcmp(w->name, "SeqClear") == 0) so.o.seq = -1; else if (strcmp(w->name, "CueClear") == 0) so.o.cue = -1;
    else if (strcmp(w->name, "ClClear") == 0) so.o.cl = -1; else if (strcmp(w->name, "sbPCueClear") == 0) so.o.pcue = -1;
    page_fill();
}

static void cl_picked(uint32_t number, void *ud) { (void)ud; lj_startup_options_set_cl((int32_t)number); }
static void on_cl_select(lj_widget *w, void *ud) { (void)w; (void)ud; lj_show_dlg_select_cuelist(cl_picked, NULL); }   /* TSelectCuelist2ModalForm picks back */

static const lj_binding lj_so_bindings[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "BOkClick", on_ok, NULL }, { "Help1Click", on_help, NULL },
    { "SeqSelectClick", on_select, (void *)(intptr_t)LJ_WIN_LIST_SEQ }, { "CueSelectClick", on_select, (void *)(intptr_t)LJ_WIN_LIST_CUE },
    { "ClSelectClick", on_cl_select, NULL }, { "sbPCueSelectClick", on_select, (void *)(intptr_t)LJ_WIN_SELECT_BGCUE },
    { "SeqClearClick", on_clear, NULL }, { "CueClearClick", on_clear, NULL }, { "ClClearClick", on_clear, NULL }, { "sbPCueClearClick", on_clear, NULL },
    { "bBrowseClick", on_browse, NULL }, { "lbautoClick", lj_handler_noop, NULL }, { "P_DeleteClick", on_delete, NULL }, { "AutoPopPopup", lj_handler_noop, NULL },
    { NULL, NULL, NULL }
};

lj_window *lj_startup_options_open(void)
{
    if (so.win) return so.win;
    so.win = lj_window_create(&lj_form_TSTARTUPFORM, lj_so_bindings);
    if (!so.win) { lj_log("startup options: cannot create TStartupForm"); return NULL; }
    lj_startup_opts_load(lj_ctx_get()->main_ini, &so.o);
    so.nlaunch = lj_auto_launch_load(so.launch, LJ_LAUNCH_MAX);
    page_fill();
    lj_win_show(so.win, 0);
    return so.win;
}

void lj_startup_options_set_cl(int32_t number) { if (so.win) { so.o.cl = number; number_field("EClNum", number); } }
int lj_startup_options_add_launch(const char *path)
{
    if (!so.win || !path || !path[0] || so.nlaunch >= LJ_LAUNCH_MAX) return -1;
    lj_str_copy(so.launch[so.nlaunch++], sizeof so.launch[0], path);
    page_fill();
    return 0;
}

static int startup_ini_path(char *dst, size_t cap) { const char *u = lj_ctx_get()->userdir; return u[0] && snprintf(dst, cap, "%sStartup.ini", u) < (int)cap ? 0 : -1; }
int lj_auto_launch_load(char (*out)[LJ_PATH_CAP], int cap)
{
    char path[LJ_PATH_CAP], key[16];
    lj_ini *ini;
    int i, n, k = 0;
    if (startup_ini_path(path, sizeof path) != 0 || !(ini = lj_ini_load(path))) return 0;
    n = lj_ini_geti(ini, "AUTOLAUNCH", "Count", 0);
    for (i = 1; i <= n && k < cap && k < LJ_LAUNCH_MAX; i++) {
        snprintf(key, sizeof key, "File%d", i);
        if (lj_str_copy(out[k], LJ_PATH_CAP, lj_ini_gets(ini, "AUTOLAUNCH", key, "")) == 0 && out[k][0]) k++;
    }
    lj_ini_free(ini);
    return k;
}
void lj_auto_launch_save(const char *const *paths, int n)
{
    char path[LJ_PATH_CAP], key[16];
    lj_ini *ini;
    int i;
    if (startup_ini_path(path, sizeof path) != 0) return;
    if (!(ini = lj_ini_load(path)) && !(ini = lj_ini_new())) return;   /* [USERDIRECTORY] / [RESTORELIB] stay as they are */
    lj_ini_seti(ini, "AUTOLAUNCH", "Count", n);
    for (i = 0; i < n; i++) { snprintf(key, sizeof key, "File%d", i + 1); lj_ini_sets(ini, "AUTOLAUNCH", key, paths[i]); }
    if (lj_ini_save(ini, path) != 0) lj_log("startup options: %s not saved", path);
    lj_ini_free(ini);
}

void lj_startup_options_close(void)
{
    if (!so.win) return;
    lj_win_hide(so.win);
    lj_window_destroy(so.win);
    so.win = NULL;
}

/* ---- W2: lj_sysprefs (lj_prefs.h); the ini bodies are complete, the TSystemPrefsForm bodies are in lj_system_prefs.c (windows-d) ---- */
typedef struct { const char *sec, *key; size_t off; int dflt; } lj_sp_key;
#define LJ_SP(f, s, k, d) { s, k, offsetof(lj_sysprefs, f), d }
static const lj_sp_key lj_sp_keys[] = {   /* the shipped User\Default\Main.Ini values are the defaults [13 s6] */
    LJ_SP(bitmap_cache, "PREFS", "BitmapCache", 0), LJ_SP(allow_freeze, "PREFS", "AllowBlind", 0), LJ_SP(allow_seq_blind, "PREFS", "AllowBlindSeq", 1),
    LJ_SP(allow_offline, "PREFS", "AllowOffline", 0), LJ_SP(allow_solo, "SOLO", "Enabled", 1), LJ_SP(open_on_lj_desktop, "PREFS", "ConfineToDesktop", 0),
    LJ_SP(open_on_windows_desktop, "PREFS", "ConfineToWindowsDesktop", 1), LJ_SP(start_on_secondary, "PREFS", "SecondaryMonitor", 0),
    LJ_SP(restore_desktop, "DESKTOP", "Restore", 0), LJ_SP(keep_seq_blind, "PREFS", "KeepSeqBlind", 1),
    LJ_SP(warn_seq_modified, "PREFS", "WarnSeq", 1), LJ_SP(warn_cue_modified, "PREFS", "WarnCue", 1), LJ_SP(warn_cl_modified, "PREFS", "WarnCueList", 1),
    LJ_SP(warn_overwrite_seq, "PREFS", "WarnOWSeq", 1), LJ_SP(warn_overwrite_cue, "PREFS", "WarnOWCue", 1), LJ_SP(warn_overwrite_cl, "PREFS", "WarnOWCueList", 1),
    LJ_SP(warn_blackout, "PREFS", "WarnBO", 1), LJ_SP(warn_freeze, "PREFS", "WarnBlind", 1), LJ_SP(warn_offline, "PREFS", "WarnOffline", 1), LJ_SP(confirm_close, "PREFS", "CloseConfirm", 1),
    LJ_SP(quick_seq, "PREFS", "QuickSeq", 0), LJ_SP(quick_cue, "PREFS", "QuickCue", 0), LJ_SP(quick_cl, "PREFS", "QuickCl", 0), LJ_SP(quick_macro, "PREFS", "QuickMacro", 0),
    LJ_SP(ext_links_01, "PREFS", "ExtMem", 0), LJ_SP(ext_links_mem_10, "PREFS", "ExtMem10", 0), LJ_SP(primary_instance, "PREFS", "Primary", 0), LJ_SP(secondary_instance, "PREFS", "Secondary", 0),
    LJ_SP(enable_offline_visualizer, "OFFLINE", "Enable43", 1), LJ_SP(enable_sandnet, "MISC", "SANDNET", 0), LJ_SP(enable_capture, "CAPTURE", "Enable", 0),
    LJ_SP(restore_password, "RESTORE", "Password", 0), LJ_SP(restore_at_startup, "RESTORE", "AtStartup", 0), LJ_SP(restore_pass_prompt, "RESTORE", "PassOnRestore", 0),
    LJ_SP(view_seqcue_bar, "PREFS", "ViewSCBar", 1), LJ_SP(view_cuelist_bar, "PREFS", "ViewClBar", 1), LJ_SP(view_fixture_bar, "PREFS", "ViewFixBar", 1), LJ_SP(view_status_bar, "PREFS", "ViewStatusBar", 1),
    LJ_SP(mark_active_fixtures, "PREFS", "MarkActiveFixtures", 0), LJ_SP(move_id, "PREFS", "MoveId", 1),
    LJ_SP(show_hotkey_warning, "MISC", "ShowHotkeyWarning", 1), LJ_SP(show_no_hardware_warning, "MISC", "ShowNoHardwareWarning", 1),
    LJ_SP(hotkeys_enabled, "HOTKEYS", "Enabled", 1), LJ_SP(hotkeys_disable_in_dialogs, "HOTKEYS", "DialogDisable", 1),
};
static uint8_t *lj_sp_field(lj_sysprefs *p, size_t off) { return (uint8_t *)p + off; }

void lj_sysprefs_defaults(lj_sysprefs *p)
{
    int i;
    memset(p, 0, sizeof *p);
    for (i = 0; i < LJ_COUNT_OF(lj_sp_keys); i++) *lj_sp_field(p, lj_sp_keys[i].off) = (uint8_t)lj_sp_keys[i].dflt;
    p->hotkey_setup = 1;
}
void lj_sysprefs_load(lj_ini *ini, lj_sysprefs *p)
{
    int i;
    const char *file;
    lj_sysprefs_defaults(p);
    if (!ini) return;
    for (i = 0; i < LJ_COUNT_OF(lj_sp_keys); i++) *lj_sp_field(p, lj_sp_keys[i].off) = (uint8_t)(lj_ini_geti(ini, lj_sp_keys[i].sec, lj_sp_keys[i].key, lj_sp_keys[i].dflt) != 0);
    file = lj_ini_gets(ini, "HOTKEYS", "File", "");           /* "Setup3.Hot" -> 3 (ours; the shipped File= is empty) */
    if (strncmp(file, "Setup", 5) == 0 && file[5] >= '1' && file[5] <= '8') p->hotkey_setup = file[5] - '0';
}
void lj_sysprefs_save(lj_ini *ini, const lj_sysprefs *p)
{
    int i;
    char file[16];
    if (!ini) return;
    for (i = 0; i < LJ_COUNT_OF(lj_sp_keys); i++) lj_ini_seti(ini, lj_sp_keys[i].sec, lj_sp_keys[i].key, ((const uint8_t *)p)[lj_sp_keys[i].off]);
    snprintf(file, sizeof file, "Setup%d.Hot", p->hotkey_setup >= 1 && p->hotkey_setup <= 8 ? p->hotkey_setup : 1);
    lj_ini_sets(ini, "HOTKEYS", "File", file);
}
