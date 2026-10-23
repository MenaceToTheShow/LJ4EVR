/* lj_extapp_form.c - the EXTAPP family (windows/lj_extapp_form.h): TExternalAppSetupForm "External Application Link",
 * the generated DFM form (lj_form_TEXTERNALAPPSETUPFORM; ljexternalconfigsetup.gif 400x346 is reduced, the DFM gives the
 * sizes). Entry: icon popup Define Application Link (PI_DefineAppLink) on a profile-90 icon -> LJ_ACT_EXTAPP_SETUP.
 * eFileName names the definition file (LJ2's plug-in .Ini; ours adds [LJ4EVR], io/lj_extapp.h); lv1 lists its keys as
 * LJ2 does; OK stores the transport in Hardware.Ini [EXTAPP] through LJ_ACT_IO_APPLY. LJ4EVR has no file browser yet
 * (the cue-list Execute File precedent), so eFileName is typed into and sbBrowse says so. The four DMX out / four DMX in
 * channels and Auto-launch / Auto-close stay greyed: nothing is executed and only the string is sent (fidelity rows).
 * Handlers: FormCreate FormDestroy sbBrowseClick bOkClick Help1Click (ME-16). */
#include "windows/lj_extapp_form.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "io/lj_extapp.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"

enum { LJ_EA_ROWS = 24 };
static struct { lj_window *win; int fixture; void (*root_orig)(lj_widget *, int, void *); char rows[LJ_EA_ROWS][LJ_EXTAPP_PATH + 32]; const char *items[LJ_EA_ROWS]; } ea = { .fixture = -1 };

static void fill_rows(const char *path)                            /* lv1: "Property\tValue" rows from the definition file */
{
    static const char *const keys[] = { "NAME", "AUTHOR", "EXECUTABLE", "PURPOSE", "BITMAP", "APPLICATIONCLASSNAME", "APPLICATIONCAPTION",
                                        "COPYDATACODE", "COMMANDINTERFACE", "FULLUPDATE", "SUPPORTSCOMMANDREQUEST", "DMXOUTCHANNELS", "DMXINCHANNELS" };
    static const char *const ours[] = { "TRANSPORT", "SERIALPORT", "BAUD", "HOST", "PORT" };
    char err[96];
    lj_ini *def = lj_extapp_def_open(path, err, sizeof err);
    lj_widget *lv = ea.win ? lj_window_find(ea.win, "lv1") : NULL;
    int n = 0;
    size_t k;
    snprintf(ea.rows[n++], sizeof ea.rows[0], "Definition File\t%s", def ? path : "Application Link Undefined");   /* exe 0x3328E0, 0x3328BC */
    for (k = 0; def && k < sizeof keys / sizeof keys[0]; k++) {
        const char *v = lj_ini_gets(def, "IDENTIFIERS", keys[k], NULL);
        if (!v) continue;
        if (k == 9 || k == 10) v = strcmp(v, "0") ? "Yes" : "No";   /* exe 0x332420 / 0x332414 */
        snprintf(ea.rows[n++], sizeof ea.rows[0], "%s\t%.200s", keys[k], v);
    }
    for (k = 0; def && k < sizeof ours / sizeof ours[0]; k++) {
        const char *v = lj_ini_gets(def, "LJ4EVR", ours[k], NULL);
        if (v) snprintf(ea.rows[n++], sizeof ea.rows[0], "%s\t%.200s", ours[k], v);
    }
    if (def) { lj_extapp_link l; memset(&l, 0, sizeof l); if (lj_extapp_def_link(def, &l, err, sizeof err) != 0) snprintf(ea.rows[n++], sizeof ea.rows[0], "LJ4EVR\t%s", err); }
    else if (path && *path) snprintf(ea.rows[n++], sizeof ea.rows[0], "LJ4EVR\t%s", err);
    lj_ini_free(def);
    for (k = 0; k < (size_t)n; k++) ea.items[k] = ea.rows[k];
    if (lv) { lj_w_listview_set_items(lv, ea.items, n); lj_widget_invalidate(lv); }
}

static void on_file(lj_widget *w, int ev, void *ud) { (void)ud; if (ev == LJ_WEV_CHANGE) fill_rows(lj_w_edit_text(w)); }
static void close_self(void) { lj_extapp_form_close(LJ_WIN_EXTAPP_SETUP); }
static void on_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) close_self(); }
static void on_root(lj_widget *w, int ev, void *ud) { if (ea.root_orig) ea.root_orig(w, ev, ud); if (ev == LJ_WEV_CLOSE) close_self(); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }
static void on_browse(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("extapp: the file browser is not available yet; type the definition file path into the field"); }

static void on_ok(lj_widget *w, void *ud)
{
    lj_extapp_cfg *cfg = lj_ctx_get()->extapp;
    const char *path = lj_w_edit_text(lj_window_find(ea.win, "eFileName"));
    lj_extapp_link l;
    char err[96];
    lj_ini *def;
    int i, rc;
    (void)w; (void)ud;
    if (!cfg || ea.fixture < 0) { close_self(); return; }
    i = lj_extapp_slot_of(cfg, ea.fixture + 1);
    if (!path || !*path) {                                         /* an empty name unlinks the fixture */
        if (i >= 0) { memset(&cfg->link[i], 0, sizeof cfg->link[i]); cfg->link[i].baud = LJ_EXTAPP_BAUD_DEFAULT; lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_EXTAPP); }
        close_self();
        return;
    }
    if (strlen(path) >= LJ_EXTAPP_PATH) { lj_win_message("External Application Link", "The definition file path is too long."); return; }
    if ((def = lj_extapp_def_open(path, err, sizeof err)) == NULL) { lj_win_message("External Application Link", err); return; }
    memset(&l, 0, sizeof l);
    l.baud = LJ_EXTAPP_BAUD_DEFAULT;
    rc = lj_extapp_def_link(def, &l, err, sizeof err);
    lj_ini_free(def);
    if (rc != 0) lj_log("extapp: %s: %s", path, err);              /* stored anyway: the link reads "Application Link Undefined" */
    if (i < 0) for (i = 0; i < LJ_SCENE_EXT && cfg->link[i].fixture; i++) {}
    if (i >= LJ_SCENE_EXT) { lj_win_message("External Application Link", "Four External Application links are defined already."); return; }
    l.fixture = (uint8_t)(ea.fixture + 1);
    lj_str_copy(l.def_file, sizeof l.def_file, path);
    cfg->link[i] = l;
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_EXTAPP);
    close_self();
}

static const lj_binding ea_b[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "sbBrowseClick", on_browse, NULL },
    { "bOkClick", on_ok, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };

static void grey(const char *name, const char *hint)
{
    lj_widget *w = lj_window_find(ea.win, name);
    if (w) { w->enabled = 0; w->hint = hint; lj_widget_invalidate(w); }
}

lj_window *lj_extapp_form_open(enum lj_win id)
{
    static const char *const greyed[] = { "seDMXOut1", "seDMXOut2", "seDMXOut3", "seDMXOut4", "seDMXIn1", "seDMXIn2", "seDMXIn3", "seDMXIn4" };
    const lj_extapp_cfg *cfg = lj_ctx_get()->extapp;
    lj_widget *w, *root;
    int i;
    size_t k;
    if (id != LJ_WIN_EXTAPP_SETUP) return NULL;
    if (ea.win) { lj_win_show(ea.win, 0); return ea.win; }
    ea.win = lj_window_create(&lj_form_TEXTERNALAPPSETUPFORM, ea_b);
    if (!ea.win) { lj_log("extapp: cannot create TExternalAppSetupForm"); return NULL; }
    root = lj_window_root(ea.win);
    ea.root_orig = root->on_event; root->on_event = on_root;
    if ((w = lj_window_find(ea.win, "BitBtn2")) != NULL) { w->on_event = on_cancel; w->ud = NULL; }   /* Kind = bkCancel, no OnClick */
    for (k = 0; k < sizeof greyed / sizeof greyed[0]; k++) grey(greyed[k], "LJ4EVR sends the command string only");
    grey("cbAutoLaunch", "LJ4EVR runs no plug-in program"); grey("cbAutoShutdown", "LJ4EVR runs no plug-in program");
    i = lj_extapp_slot_of(cfg, ea.fixture + 1);
    if ((w = lj_window_find(ea.win, "eFileName")) != NULL) {
        ((lj_w_edit *)w)->readonly = 0;                            /* typed: no file browser yet */
        lj_w_edit_set_text(w, i >= 0 ? cfg->link[i].def_file : "");
        w->on_event = on_file; w->ud = NULL;
    }
    fill_rows(i >= 0 ? cfg->link[i].def_file : "");
    lj_win_show(ea.win, 0);
    return ea.win;
}

void lj_extapp_form_close(enum lj_win id)
{
    if (id != LJ_WIN_EXTAPP_SETUP || !ea.win) return;
    lj_win_hide(ea.win);
    lj_window_destroy(ea.win);
    ea.win = NULL; ea.root_orig = NULL;
}

lj_window *lj_extapp_form_get(enum lj_win id) { return id == LJ_WIN_EXTAPP_SETUP ? ea.win : NULL; }
void       lj_extapp_form_refresh(enum lj_win id) { (void)id; }
void       lj_extapp_form_bind_fns(void) {}                        /* LJ_ACT_EXTAPP_SETUP is bound in app/lj_app_io.c */
void       lj_extapp_form_target(int fixture) { ea.fixture = fixture >= 0 && fixture < LJ_FIXTURES ? fixture : -1; }
