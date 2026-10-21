/* lj_windows.c - W1 contract stub (lj_windows.h); the windows tracks replace the
 * open/close/refresh bodies (one behaviour file per window) and keep the caption
 * helpers, which are complete here (F19, 13 s5 literals). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_windows.h"
#include "core/lj_log.h"
#include "windows/lj_lists.h"
#include "windows/lj_save_dialogs.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_dmx_monitor.h"       /* windows-c */
#include "windows/lj_fixture_config.h"
#include "windows/lj_hardware_setup.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"

static const char *const lj_win_classes[LJ_WIN_COUNT] = {
    "TSequenceForm", "TCurrentCue", "TSaveSeqForm", "TSaveCueForm", "TSeqListForm", "TCueListForm", "TLinkConfigForm3" /* windows-c: the DFM class (no TFixtureConfigForm exists) */,
    "TMasterIntensityForm", "THardwareSetupForm2" /* windows-c: the DFM class */, "TDMXOutForm", "TStartupForm", "TStatusWarningForm", "TCloseOrSaveForm" /* windows-c: "Close Lightjockey ?" is this form; TConfirmForm is "Please Confirm.." */, "TStatusForm" };
static const char *const lj_win_ids[LJ_WIN_COUNT] = {
    "seq_form", "cue_form", "save_seq", "save_cue", "list_seq", "list_cue", "fixture_config",
    "master_intensity", "hardware_setup", "dmx_monitor", "startup_options", "warnings", "close_prompt", "please_wait" };

/* windows-a: the dispatch, one row per window id naming its behaviour file; windows-b/c add their rows by index
 * (a row left empty = that track has not landed: open logs and returns NULL). */
typedef struct { lj_window *(*open)(void); void (*close)(void); lj_window *(*get)(void); void (*refresh)(void); } lj_win_ops;
static const lj_win_ops lj_win_table[LJ_WIN_COUNT] = {
    [LJ_WIN_SEQ_FORM] = { lj_seq_form_open, lj_seq_form_close, lj_seq_form_get, lj_seq_form_refresh },
    [LJ_WIN_SAVE_SEQ] = { lj_save_seq_open, lj_save_seq_close, lj_save_seq_get, lj_save_seq_refresh },
    [LJ_WIN_LIST_SEQ] = { lj_seqlist_open, lj_seqlist_close, lj_seqlist_get, lj_seqlist_refresh },
    /* windows-c */
    [LJ_WIN_FIXTURE_CONFIG]  = { lj_fixture_config_open, lj_fixture_config_close, lj_fixture_config_window, lj_fixture_config_refresh },
    [LJ_WIN_HARDWARE_SETUP]  = { lj_hardware_setup_open, lj_hardware_setup_close, lj_hardware_setup_window, lj_hardware_setup_refresh },
    [LJ_WIN_DMX_MONITOR]     = { lj_dmx_monitor_open, lj_dmx_monitor_close, lj_dmx_monitor_window, lj_dmx_monitor_refresh },
    [LJ_WIN_STARTUP_OPTIONS] = { lj_startup_options_open, lj_startup_options_close, lj_startup_options_window, NULL },
    [LJ_WIN_WARNINGS]        = { NULL /* lj_win_warning(state, 1) */, lj_warnings_close_all, lj_win_warnings_any, NULL },
    [LJ_WIN_CLOSE_PROMPT]    = { NULL /* lj_win_close_prompt(cb) */, lj_warnings_close_all, lj_win_close_prompt_window, NULL },
};
static const lj_win_ops *lj_win_ops_of(enum lj_win id) { return (int)id >= 0 && id < LJ_WIN_COUNT ? &lj_win_table[id] : NULL; }

lj_window *lj_win_open(enum lj_win id)
{
    const lj_win_ops *o = lj_win_ops_of(id);
    if (!o || !o->open) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
    return o->open();
}
void lj_win_close(enum lj_win id) { const lj_win_ops *o = lj_win_ops_of(id); if (o && o->close) o->close(); }
void lj_win_toggle(enum lj_win id) { if (lj_win_get(id)) lj_win_close(id); else lj_win_open(id); }
lj_window *lj_win_get(enum lj_win id) { const lj_win_ops *o = lj_win_ops_of(id); return o && o->get ? o->get() : NULL; }
void lj_win_refresh(enum lj_win id) { const lj_win_ops *o = lj_win_ops_of(id); if (o && o->refresh) o->refresh(); }
void lj_win_refresh_all(void) { int i; for (i = 0; i < LJ_WIN_COUNT; i++) lj_win_refresh((enum lj_win)i); }
void lj_win_close_all(void) { int i; for (i = 0; i < LJ_WIN_COUNT; i++) lj_win_close((enum lj_win)i); }
lj_window *lj_win_find(const char *form)
{
    int i;
    if (!form) return NULL;
    for (i = 0; i < LJ_WIN_COUNT; i++)
        if (strcmp(form, lj_win_classes[i]) == 0 || strcmp(form, lj_win_ids[i]) == 0) return lj_win_get((enum lj_win)i);
    return NULL;
}
const char *lj_win_class(enum lj_win id) { return ((int)id >= 0 && id < LJ_WIN_COUNT) ? lj_win_classes[id] : ""; }
static const lj_win_host *lj_win_the_host;   /* windows-c: stored so the behaviour files can reach the screen */
void lj_win_set_host(const lj_win_host *host) { lj_win_the_host = host; lj_seq_form_bind_fns(); }

static int lj_caption(char *dst, size_t cap, const char *prefix, uint32_t number, const char *name, const char *suffix)
{
    int n;
    if (!dst || cap == 0) return -1;
    n = snprintf(dst, cap, "%s[%04u %s]%s", prefix, (unsigned)number, (name && name[0]) ? name : "No Name", suffix);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}
int lj_win_caption_seq(char *dst, size_t cap, uint32_t number, const char *name) { return lj_caption(dst, cap, "SEQUENCE ", number, name, ""); }
int lj_win_caption_cue(char *dst, size_t cap, uint32_t number, const char *name, int edit) { return lj_caption(dst, cap, edit ? "EDIT CUE " : "CUE ", number, name, ""); }
int lj_win_caption_cuelist(char *dst, size_t cap, uint32_t number, const char *name, int tracking) { return lj_caption(dst, cap, "Cuelist ", number, name, tracking ? " Tracking on" : ""); }
int lj_win_caption_main(char *dst, size_t cap, const char *library)
{
    int n;
    if (!dst || cap == 0) return -1;
    n = snprintf(dst, cap, "Martin LightJockey (User lib: %s)", library ? library : "Default");
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* lj_win_warning / lj_win_close_prompt / lj_win_message: windows-c bodies in lj_warnings.c */
const char *lj_win_id_name(enum lj_win id) { return ((int)id >= 0 && id < LJ_WIN_COUNT) ? lj_win_ids[id] : ""; }

/* ---- windows-c additions (appended): the host accessor and the two show/hide helpers every behaviour file uses ---- */
const lj_win_host *lj_win_host_get(void) { return lj_win_the_host; }
int  lj_win_show(lj_window *w, unsigned flags) { return (w && lj_win_the_host && lj_win_the_host->open) ? lj_win_the_host->open(lj_win_the_host->ud, w, flags) : 0; }
void lj_win_hide(lj_window *w) { if (w && lj_win_the_host && lj_win_the_host->close) lj_win_the_host->close(lj_win_the_host->ud, w); }
