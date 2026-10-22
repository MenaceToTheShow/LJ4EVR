/* lj_system_prefs.c - see lj_system_prefs.h. Every OnXxx of TSYSTEMPREFSFORM is bound (ME-16). */
#include "windows/lj_system_prefs.h"

#include <stddef.h>
#include <string.h>
#include "core/lj_log.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"

typedef struct { const char *box; size_t off; const char *sec, *key; } lj_sp_box;
#define LJ_BOX(b, f, s, k) { b, offsetof(lj_sysprefs, f), s, k }
static const lj_sp_box lj_sp_boxes[] = {   /* box -> field -> the Main.Ini key lj_sysprefs_save writes (windows/lj_prefs.h) */
    /* System Settings */
    LJ_BOX("cbBitmapCache", bitmap_cache, "PREFS", "BitmapCache"), LJ_BOX("cbAllowFreeze", allow_freeze, "PREFS", "AllowBlind"),
    LJ_BOX("cbAllowSeqBlind", allow_seq_blind, "PREFS", "AllowBlindSeq"), LJ_BOX("cbAllowOffline", allow_offline, "PREFS", "AllowOffline"),
    LJ_BOX("cbAllowSolo", allow_solo, "SOLO", "Enabled"), LJ_BOX("cbOpenOnDesktop", open_on_lj_desktop, "PREFS", "ConfineToDesktop"),
    LJ_BOX("cbOpenOnWindowsDesktop", open_on_windows_desktop, "PREFS", "ConfineToWindowsDesktop"), LJ_BOX("cbStartOnSecondary", start_on_secondary, "PREFS", "SecondaryMonitor"),
    LJ_BOX("cbRestoreLJDesktop", restore_desktop, "DESKTOP", "Restore"),
    /* Warnings */
    LJ_BOX("SeqWarn", warn_seq_modified, "PREFS", "WarnSeq"), LJ_BOX("CueWarn", warn_cue_modified, "PREFS", "WarnCue"),
    LJ_BOX("cbOseqWarn", warn_overwrite_seq, "PREFS", "WarnOWSeq"), LJ_BOX("cbOCueWarn", warn_overwrite_cue, "PREFS", "WarnOWCue"),
    LJ_BOX("cbBOWarn", warn_blackout, "PREFS", "WarnBO"), LJ_BOX("cbFreezeWarn", warn_freeze, "PREFS", "WarnBlind"),
    LJ_BOX("cbOfflineWarn", warn_offline, "PREFS", "WarnOffline"), LJ_BOX("cbCloseWarn", confirm_close, "PREFS", "CloseConfirm"),
    /* Quick Save/Load */
    LJ_BOX("cbQuickSeq", quick_seq, "PREFS", "QuickSeq"), LJ_BOX("cbQuickCue", quick_cue, "PREFS", "QuickCue"),
    LJ_BOX("cbQuickCl", quick_cl, "PREFS", "QuickCl"), LJ_BOX("cbQuickMacro", quick_macro, "PREFS", "QuickMacro"),
    /* External Links (inert, ME-31; the last three greyed, D12) */
    LJ_BOX("cbExternal01", ext_links_01, "PREFS", "ExtMem"), LJ_BOX("cbExternal10", ext_links_mem_10, "PREFS", "ExtMem10"),
    LJ_BOX("cbPrimary", primary_instance, "PREFS", "Primary"), LJ_BOX("cbSecondary", secondary_instance, "PREFS", "Secondary"),
    LJ_BOX("cbEnableOffline43", enable_offline_visualizer, "OFFLINE", "Enable43"), LJ_BOX("cbSandnet", enable_sandnet, "MISC", "SANDNET"),
    LJ_BOX("cbCapture", enable_capture, "CAPTURE", "Enable"),
    /* Data Restore (the boxes persist; the restore data itself is W4) */
    LJ_BOX("cbEnablePassword", restore_password, "RESTORE", "Password"), LJ_BOX("cbEnableatStartup", restore_at_startup, "RESTORE", "AtStartup"),
    LJ_BOX("cbPassOnRestore", restore_pass_prompt, "RESTORE", "PassOnRestore"),
};
/* not on the dialog (the struct carries them for the same owner): warn_cl_modified, warn_overwrite_cl (LJ2's Warnings tab has no cue list boxes),
 * the view_* bars, mark_active_fixtures, move_id, show_*_warning, hotkeys_*; CbPrompt "Prompt before restoring" is hidden in the DFM and has no key. */

static struct { lj_window *win; lj_sysprefs page; int close_pending, guard; } sp;

static uint8_t *field(lj_sysprefs *p, size_t off) { return (uint8_t *)p + off; }
static lj_widget *find(const char *n) { return lj_window_find(sp.win, n); }

int lj_system_prefs_box_count(void) { return LJ_COUNT_OF(lj_sp_boxes); }
const char *lj_system_prefs_box(int i, const char **sec, const char **key)
{
    if (i < 0 || i >= LJ_COUNT_OF(lj_sp_boxes)) return NULL;
    if (sec) *sec = lj_sp_boxes[i].sec;
    if (key) *key = lj_sp_boxes[i].key;
    return lj_sp_boxes[i].box;
}

static void page_fill(void)
{
    int i;
    sp.guard = 1;
    for (i = 0; i < LJ_COUNT_OF(lj_sp_boxes); i++) lj_w_checkbox_set_checked(find(lj_sp_boxes[i].box), *field(&sp.page, lj_sp_boxes[i].off));
    sp.guard = 0;
}

static void page_read(void)
{
    int i;
    for (i = 0; i < LJ_COUNT_OF(lj_sp_boxes); i++) { lj_widget *b = find(lj_sp_boxes[i].box); if (b) *field(&sp.page, lj_sp_boxes[i].off) = (uint8_t)lj_w_checkbox_checked(b); }
}

int lj_system_prefs_apply(void)
{
    lj_ctx *c = lj_ctx_get();
    if (!sp.win || !c->prefs) return -1;
    page_read();
    *c->prefs = sp.page;
    lj_desktop_run_action(LJ_ACT_SYSTEM_PREFS_APPLY, 0);   /* the app: Main.Ini, toolbar gates, LJ_CMD_PREFS (seam 3) */
    return 0;
}

/* the exclusive pairs: checking one clears the other */
static void exclusive(lj_widget *w, const char *a, const char *b)
{
    if (sp.guard || !w || !w->name || !lj_w_checkbox_checked(w)) return;
    sp.guard = 1;
    lj_w_checkbox_set_checked(find(strcmp(w->name, a) == 0 ? b : a), 0);
    sp.guard = 0;
}
static void on_confine(lj_widget *w, void *ud) { (void)ud; exclusive(w, "cbOpenOnDesktop", "cbOpenOnWindowsDesktop"); }
static void on_instance(lj_widget *w, void *ud) { (void)ud; exclusive(w, "cbPrimary", "cbSecondary"); }
static void w4(lj_widget *w, void *ud) { (void)ud; lj_log("system prefs: %s: the Data Restore data is W4", w && w->name ? w->name : "?"); }
static void on_ok(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_system_prefs_apply() == 0) sp.close_pending = 1; }
static void on_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; sp.close_pending = 1; }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("system prefs: Help is W4"); }

static const lj_binding lj_sp_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { "PC1Change", lj_handler_noop, NULL },
    { "cbOpenOnDesktopClick", on_confine, NULL }, { "cbExternal01Click", lj_handler_noop, NULL }, { "cbExternal10Click", lj_handler_noop, NULL },
    { "cbPrimaryClick", on_instance, NULL }, { "cbSecondaryClick", on_instance, NULL }, { "cbClick", lj_handler_noop, NULL }, { "cbSandnetClick", lj_handler_noop, NULL }, { "cbCaptureClick", lj_handler_noop, NULL },
    { "bCreateClick", w4, NULL }, { "bNewPasswordClick", w4, NULL }, { "ePasswordKeyDown", lj_handler_noop, NULL }, { "ValidatePassword", w4, NULL },
    { "BCancelClick", on_cancel, NULL }, { "BokClick", on_ok, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_system_prefs_open(void)
{
    static const char *const greyed[] = { "cbEnableOffline43", "cbSandnet", "cbCapture" };
    lj_ctx *c = lj_ctx_get();
    int i;
    if (sp.win) return sp.win;
    sp.win = lj_window_create(&lj_form_TSYSTEMPREFSFORM, lj_sp_bindings);
    if (!sp.win) { lj_log("system prefs: cannot create TSystemPrefsForm"); return NULL; }
    sp.close_pending = 0;
    if (c->prefs) sp.page = *c->prefs; else lj_sysprefs_defaults(&sp.page);
    page_fill();
    for (i = 0; i < LJ_COUNT_OF(greyed); i++) { lj_widget *b = find(greyed[i]); if (b) b->enabled = 0; }   /* D12: no transmitter hardware here */
    lj_w_tabs_select(find("PC1"), 0);                                                                        /* opens on the first tab (DFM ActivePage = TabSheet4 is the designer's) */
    lj_win_show(sp.win, 0);
    return sp.win;
}

void lj_system_prefs_close(void)
{
    if (!sp.win) return;
    lj_win_hide(sp.win);
    lj_window_destroy(sp.win);
    sp.win = NULL; sp.close_pending = 0;
}

lj_window *lj_system_prefs_window(void) { return sp.win; }
void lj_system_prefs_refresh(void) { if (sp.close_pending) lj_system_prefs_close(); }
