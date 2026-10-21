/* lj_app.c - lj4evr startup [01 s6.2], the UI loop's idle pass, the record-loading
 * seam (design 2.3 rules 2, 3, 6), the 100 ms refresh, the close prompt and the
 * shutdown order (ME-23). The only app file that includes platform/. */
#include "app/lj_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_prefs.h"
#include "app/lj_startup.h"
#include "app/lj_uiscript.h"
#include "controls/lj_ctl.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_main_window.h"
#include "engine/lj_engine.h"
#include "gfx/lj_bmp.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"
#include "io/lj_net.h"
#include "io/lj_sacn.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "platform/lj_platform.h"
#include "platform/sdl/lj_sdl.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_formsini.h"
#include "ui/lj_menu.h"
#include "ui/lj_window.h"
#include "windows/lj_windows.h"
#include "version.h"

/* ponytail: one process, one app; the lj_app_* entry points of lj_app.h carry no handle. */
static struct {
    lj_platform_cfg cfg;
    char pref[LJ_PATH_CAP];                 /* SDL's pref path, UTF-8 bytes as given (ME-65) */
    lj_library lib;
    lj_sacn_cfg sacn;
    lj_hotkeys hotkeys;
    lj_window *main, *status_own, *seq_fb, *cue_fb;   /* *_fb: W0 fallback windows while windows/ has no body */
    int main_is_desktop;
    lj_engine_status view;                  /* the one lj_engine_view copy per pass (seam 3) */
    lj_frame frame;                         /* the 100 ms copy */
    int explode, run_ms, selftest, tick_hz, rc, no_save, cue_edit, closing;
    const char *save_path, *selftest_ref, *script_path, *u_arg;
    uint64_t t0, last_refresh;
    void (*main_on_event)(lj_widget *w, int ev, void *ud);   /* the desktop's root handler, wrapped for the close prompt */
} lj_a;

/* ---- W0 fallback main window bindings (ME-38): kept until desktop/ builds the main window itself ---- */
static void lj_app_menu_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
#define LJ_NOOP(name) { name, lj_app_menu_noop, NULL }
static const lj_binding lj_app_main_bindings[] = {
    LJ_NOOP("FormClose"),                /* the close prompt is handled by the root wrapper below */
    LJ_NOOP("AutoStrikeLamps1Click"), LJ_NOOP("Config21Click"), LJ_NOOP("DefaultCurrentScene1Click"), LJ_NOOP("DeleteFilesClick"),
    LJ_NOOP("Fingers1Click"), LJ_NOOP("GlobalPatch1Click"), LJ_NOOP("Hardware2Click"), LJ_NOOP("HelpClick"), LJ_NOOP("M_AudioClick"),
    LJ_NOOP("M_AudioOptionsClick"), LJ_NOOP("M_CaptureClick"), LJ_NOOP("M_Downloadclick"), LJ_NOOP("M_EditHotkeysClick"), LJ_NOOP("M_FingersClick"),
    LJ_NOOP("M_FollowSpotClick"), LJ_NOOP("M_GenericClick"), LJ_NOOP("M_HotkeysClick"), LJ_NOOP("M_HotkeysEnabledClick"), LJ_NOOP("M_ManagerClick"),
    LJ_NOOP("M_OffLineClick"), LJ_NOOP("M_OfflineDisableClick"), LJ_NOOP("M_OfflineLaunchClick"), LJ_NOOP("M_OfflinePrefs"), LJ_NOOP("M_RestartLightJockeyClick"),
    LJ_NOOP("M_RestoreProgramDataClick"), LJ_NOOP("M_SandNetStatusClick"), LJ_NOOP("M_SystemClick"), LJ_NOOP("M_UserLibrariesClick"), LJ_NOOP("MenuViewClick"),
    LJ_NOOP("NextPreviousFixtureSelection1Click"), LJ_NOOP("SearchHelp1Click"), LJ_NOOP("SelectHelpFile"), LJ_NOOP("SelectHelpfile1Click"), LJ_NOOP("StartUp1Click"),
    LJ_NOOP("SystemPrefsClick"), LJ_NOOP("Toolbars1Click"), LJ_NOOP("ViewControlClick"),
    { NULL, NULL, NULL } };

/* ---- windows host: how a created lj_window reaches the screen (lj_windows.h) ---- */

/* --explode (design 7.6): a container declared after the controls it covers moves below them, the client grows. */
static void lj_app_explode(lj_window *win)
{
    lj_widget *root = lj_window_root(win), *c, *p;
    lj_rect cr;
    int bottom, max_bottom = 0;
    if (!root) return;
    for (c = root->first; c; c = c->next) {
        if (c->kind == LJ_K_PANEL || c->kind == LJ_K_GROUPBOX || c->kind == LJ_K_PAGECONTROL) {
            bottom = c->y;
            for (p = root->first; p != c; p = p->next)
                if (p->x < c->x + c->w && c->x < p->x + p->w && p->y < c->y + c->h && c->y < p->y + p->h && p->y + p->h + 4 > bottom)
                    bottom = p->y + p->h + 4;
            c->y = bottom;
        }
        if (c->y + c->h > max_bottom) max_bottom = c->y + c->h;
    }
    lj_window_client_rect(win, &cr);
    if (max_bottom > cr.h) lj_window_resize(win, cr.w, max_bottom);
    lj_window_invalidate_all(win);
}

static int lj_app_host_open(void *ud, lj_window *w, unsigned flags)
{
    lj_ctx *c = lj_ctx_get();
    int x, y, mx, my;
    (void)ud;
    if (lj_a.explode) lj_app_explode(w);
    if (lj_platform_open_window(w, flags) != 0) return -1;
    if (lj_prefs_window_pos(c->forms_ini, c->main_ini, w, &x, &y) != 0) {   /* never persisted or first open: centre on the main window (7.10) */
        const lj_form_desc *d = lj_window_desc(w);
        const lj_surface *ms = lj_window_surface(lj_a.main), *ws = lj_window_surface(w);
        x = d ? d->left : 0; y = d ? d->top : 0;
        if (lj_a.main && lj_platform_window_pos(lj_a.main, &mx, &my) == 0) lj_formsini_center(mx, my, ms->w, ms->h, ws->w, ws->h, &x, &y);
    }
    lj_platform_move_window(w, x, y);
    return 0;
}

static void lj_app_host_close(void *ud, lj_window *w)
{
    lj_ctx *c = lj_ctx_get();
    int x, y;
    (void)ud;
    if (!w) return;
    if (!lj_a.no_save && lj_platform_window_pos(w, &x, &y) == 0) lj_prefs_window_save(c->forms_ini, c->main_ini, w, x, y);
    lj_platform_close_window(w);
}

static void lj_app_host_show(void *ud, lj_window *w, int show) { (void)ud; lj_platform_show_window(w, show); }

static const lj_win_host lj_app_host = { NULL, lj_app_host_open, lj_app_host_close, lj_app_host_show };

/* W0 fallback for a generated form while lj_win_open has no body (ponytail: deleted with the windows track). */
static lj_window *lj_app_open_form(const lj_form_desc *d)
{
    lj_window *win = lj_window_create(d, NULL);
    if (!win) return NULL;
    if (lj_app_host_open(NULL, win, 0) != 0) { lj_window_destroy(win); return NULL; }
    return win;
}

static void lj_app_close_form(lj_window **win)
{
    if (!*win) return;
    lj_app_host_close(NULL, *win);
    lj_window_destroy(*win);
    *win = NULL;
}

/* ---- the close prompt (TCONFIRMFORM "Close Lightjockey ?") ---- */

static void lj_app_on_close_result(int yes, void *ud)
{
    (void)ud;
    lj_a.closing = 0;
    if (yes) lj_app_quit();
}

static void lj_app_main_event(lj_widget *w, int ev, void *ud)
{
    if (ev == LJ_WEV_CLOSE) {
        if (lj_uiscript_active()) { lj_app_quit(); return; }          /* a script never prompts (docs/ui-script.md) */
        if (lj_a.closing) return;
        lj_a.closing = 1;
        lj_win_close_prompt(lj_app_on_close_result, NULL);
        return;
    }
    if (lj_a.main_on_event) lj_a.main_on_event(w, ev, ud);
}

/* ---- the idle pass and the 100 ms hook ---- */

void lj_app_quit(void) { lj_platform_quit(); }

static const lj_engine_status *lj_app_view(void *ud) { (void)ud; return &lj_a.view; }

static lj_window *lj_app_resolve(void *ud, const char *form)
{
    lj_window *w = lj_uiscript_resolve_default(form);
    (void)ud;
    if (w) return w;
    if (strcmp(form, "main") == 0) return lj_a.main;
    if (lj_a.seq_fb && (strcmp(form, "TSequenceForm") == 0 || strcmp(form, "seq_form") == 0)) return lj_a.seq_fb;
    if (lj_a.cue_fb && (strcmp(form, "TCurrentCue") == 0 || strcmp(form, "cue_form") == 0)) return lj_a.cue_fb;
    return NULL;
}

void lj_app_service_pending(void)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t link, bg, gm;
    if (!c->engine) return;
    lj_lock(); lj_engine_view(c->engine, &lj_a.view); lj_unlock();
    link = lj_a.view.pending_link_cue; bg = lj_a.view.pending_bg; gm = lj_a.view.pending_genmacro;
    if (link) lj_app_load_cue(link, 0);
    if (bg) lj_app_load_bgcue(bg);
    if (gm) lj_app_load_genmacro(gm);
}

void lj_app_refresh(void)
{
    lj_ctx *c = lj_ctx_get();
    const lj_sequence *s = c->show ? c->show->current_seq : NULL;
    const lj_cue *q = c->show ? c->show->current_cue : NULL;
    if (c->engine) { lj_lock(); memcpy(&lj_a.frame, lj_engine_frame(c->engine), sizeof lj_a.frame); lj_unlock(); }
    lj_desktop_seqcue_fields(s ? s->number : 0, s ? s->name : "", c->current_scene + 1, s ? s->n : 0, q ? q->number : 0, q ? q->name : "");
    lj_desktop_status_flag(LJ_SB_SEQ_MOD, c->modified[LJ_MOD_SEQ]);
    lj_desktop_status_flag(LJ_SB_CUE_MOD, c->modified[LJ_MOD_CUE]);
    lj_desktop_status_flag(LJ_SB_CL_MOD, c->modified[LJ_MOD_CL]);
    lj_win_warning(LJ_WARN_BLACKOUT, lj_a.view.blackout);
    lj_win_warning(LJ_WARN_FROZEN, lj_a.view.freeze);
    lj_win_warning(LJ_WARN_FADE_OUT, lj_a.view.gm_fade == 1);
    lj_win_warning(LJ_WARN_FADE_IN, lj_a.view.gm_fade == 2);
    lj_win_warning(LJ_WARN_OFFLINE, lj_a.view.offline);
    lj_desktop_refresh();
    lj_win_refresh_all();
    lj_ctl_refresh();
}

static void lj_app_idle(void *ud)
{
    uint64_t now;
    int r;
    (void)ud;
    if (lj_uiscript_active()) {
        r = lj_uiscript_step();
        if (r < 0) { lj_a.rc = 1; lj_a.no_save = 1; lj_app_quit(); }   /* failure: exit 1, nothing saved to forms.ini */
        else if (r == 0) lj_app_quit();                                   /* quit / EOF: a normal close without the prompt */
    }
    lj_app_service_pending();
    now = lj_clock_us();
    if (now - lj_a.last_refresh >= 100000u) { lj_a.last_refresh = now; lj_app_refresh(); }   /* the 100 ms cadence [11 E24] */
    if (lj_a.run_ms > 0 && (now - lj_a.t0) / 1000u >= (uint64_t)lj_a.run_ms) lj_app_quit();
}

/* ---- the record-loading seam (design 2.3 rules 2 and 6) ---- */

typedef struct { lj_sequence *seq[LJ_CUE_SLOTS + LJ_BG_SLOTS]; int n; } lj_app_fresh;   /* records loaded from disk this call, inserted under the lock */

static int lj_app_rec_path(enum lj_rec_kind kind, uint32_t number, char *out, size_t cap)
{
    if (lj_library_record_path(&lj_a.lib, kind, number, out, cap) == 0) return 0;
    lj_log("app: no path for record kind %d number %u", (int)kind, (unsigned)number);
    return -1;
}

/* The pinned pointer for a sequence number: the table's record, else loaded from disk now; NULL = load error [03 s5.1]. */
static const lj_sequence *lj_app_seq_for(uint32_t number, lj_app_fresh *fresh)
{
    lj_show *show = lj_ctx_get()->show;
    lj_sequence *s;
    char path[LJ_PATH_CAP], err[128];
    int i;
    if (!number) return NULL;
    if ((s = (lj_sequence *)lj_table_get(&show->sequences, number)) != NULL) return s;
    for (i = 0; i < fresh->n; i++) if (fresh->seq[i]->number == number) return fresh->seq[i];
    if (fresh->n >= LJ_COUNT_OF(fresh->seq) || lj_app_rec_path(LJ_REC_SEQ, number, path, sizeof path) != 0) return NULL;
    s = lj_sequence_new_rec();
    if (lj_sequence_load(path, s, err, sizeof err) != 0) { lj_log("sequence %04u: load error: %s", (unsigned)number, err); lj_sequence_free(s); return NULL; }
    s->number = number;
    fresh->seq[fresh->n++] = s;
    return s;
}

static lj_bgcue *lj_app_bg_for(uint32_t number, lj_cmd *cmd, lj_app_fresh *fresh)
{
    char path[LJ_PATH_CAP], err[128];
    lj_bgcue *bg;
    int i;
    if (!number || lj_app_rec_path(LJ_REC_BG, number, path, sizeof path) != 0) return NULL;
    bg = lj_bgcue_new_rec();
    if (lj_bgcue_load(path, bg, err, sizeof err) != 0) { lj_log("background cue %04u: load error: %s", (unsigned)number, err); lj_bgcue_free(bg); return NULL; }
    bg->number = number;
    cmd->bg = bg;
    for (i = 0; i < LJ_BG_SLOTS; i++) cmd->bgseq[i] = lj_app_seq_for(bg->slot[i].seq, fresh);
    return bg;
}

/* Caller holds lj_lock: the fresh records enter the tables and the command is appended in the same critical section (rule 6). */
static void lj_app_commit(lj_app_fresh *fresh, lj_cmd *cmd)
{
    lj_ctx *c = lj_ctx_get();
    int i;
    for (i = 0; i < fresh->n; i++) lj_table_put(&c->show->sequences, fresh->seq[i]->number, fresh->seq[i]);
    if (cmd->at_us == 0) cmd->at_us = lj_clock_us();
    if (c->engine) lj_engine_cmd(c->engine, cmd);
}

/* Caller holds lj_lock: re-pins the current cue from the tables (after a sequence was replaced or deleted); exclude = a number now gone (slot -> load error). */
static void lj_app_repin_cue(uint32_t exclude)
{
    lj_show *show = lj_ctx_get()->show;
    const lj_cue *cue = show->current_cue;
    lj_cmd cmd;
    lj_bgcue *bg;
    int i;
    if (!cue) return;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = cue->number; cmd.b = (uint32_t)lj_a.cue_edit; cmd.cue = cue;
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        uint32_t n = cue->slot[i].seq;
        cmd.seq[i] = (n && n != exclude) ? (const lj_sequence *)lj_table_get(&show->sequences, n) : NULL;
    }
    if (cue->bg_cue && (bg = (lj_bgcue *)lj_table_get(&show->bgcues, cue->bg_cue)) != NULL) {
        cmd.bg = bg;
        for (i = 0; i < LJ_BG_SLOTS; i++) cmd.bgseq[i] = bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&show->sequences, bg->slot[i].seq) : NULL;
    }
    cmd.at_us = lj_clock_us();
    if (lj_ctx_get()->engine) lj_engine_cmd(lj_ctx_get()->engine, &cmd);
}

int lj_app_load_cue(uint32_t number, int edit)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = c->show;
    lj_cue *cue, *old, *oldcur;
    lj_bgcue *bg = NULL, *oldbg = NULL;
    lj_app_fresh fresh;
    lj_cmd cmd;
    char path[LJ_PATH_CAP], err[128];
    int i;

    if (!show || !number || lj_app_rec_path(LJ_REC_CUE, number, path, sizeof path) != 0) return -1;
    cue = lj_cue_new_rec();
    if (lj_cue_load(path, cue, err, sizeof err) != 0) { lj_log("cue %04u: load error: %s", (unsigned)number, err); lj_cue_free(cue); return -1; }
    cue->number = number;
    memset(&fresh, 0, sizeof fresh);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = number; cmd.b = (uint32_t)(edit != 0); cmd.cue = cue;
    for (i = 0; i < LJ_CUE_SLOTS; i++) cmd.seq[i] = lj_app_seq_for(cue->slot[i].seq, &fresh);
    if (cue->bg_cue) bg = lj_app_bg_for(cue->bg_cue, &cmd, &fresh);
    cmd.gm = NULL;                                                /* generic macros are W2 */

    lj_lock();
    old = (lj_cue *)lj_table_get(&show->cues, number);
    lj_table_put(&show->cues, number, cue);
    oldcur = show->current_cue;
    show->current_cue = cue;
    if (bg) { oldbg = (lj_bgcue *)lj_table_get(&show->bgcues, bg->number); lj_table_put(&show->bgcues, bg->number, bg); }
    lj_app_commit(&fresh, &cmd);
    lj_unlock();
    /* freed after the engine was handed the replacement in the same critical section (rule 6); a never-saved current cue is not in the table */
    if (old && old != cue) lj_cue_free(old);
    if (oldcur && oldcur != cue && oldcur != old && lj_table_get(&show->cues, oldcur->number) != oldcur) lj_cue_free(oldcur);
    if (oldbg && oldbg != bg) lj_bgcue_free(oldbg);
    lj_a.cue_edit = edit != 0;
    c->modified[LJ_MOD_CUE] = 0; c->cue_saved = 1;
    return 0;
}

int lj_app_load_bgcue(uint32_t number)
{
    lj_show *show = lj_ctx_get()->show;
    lj_bgcue *bg = NULL, *old = NULL;
    lj_app_fresh fresh;
    lj_cmd cmd;
    if (!show) return -1;
    memset(&fresh, 0, sizeof fresh);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_BG; cmd.a = number;
    if (number && (bg = lj_app_bg_for(number, &cmd, &fresh)) == NULL) return -1;   /* number 0 = clear */
    lj_lock();
    if (bg) { old = (lj_bgcue *)lj_table_get(&show->bgcues, number); lj_table_put(&show->bgcues, number, bg); }
    lj_app_commit(&fresh, &cmd);
    lj_unlock();
    if (old && old != bg) lj_bgcue_free(old);
    return 0;
}

int lj_app_load_genmacro(uint32_t number)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_GENMACRO; cmd.a = number; cmd.gm = NULL;     /* W2: no loader yet; NULL = engine off for this cue */
    lj_log("generic macro %04u: generic macros are W2, macro off", (unsigned)number);
    lj_ctx_post(&cmd);
    return -1;
}

/* Swaps the current sequence under the lock, posts LJ_CMD_CURRENT_SEQ, returns the old record for the caller to free. */
static lj_sequence *lj_app_set_current_seq(lj_sequence *s)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *old;
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_CURRENT_SEQ; cmd.a = 0; cmd.b = c->blind; cmd.seq[0] = s; cmd.at_us = lj_clock_us();   /* seq[0] = the pinned current sequence (lj_engine.h table) */
    lj_lock();
    old = c->show->current_seq;
    c->show->current_seq = s;
    c->current_scene = 0;
    if (c->engine) lj_engine_cmd(c->engine, &cmd);
    lj_unlock();
    return old;
}

int lj_app_load_sequence(uint32_t number)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s, *old;
    char path[LJ_PATH_CAP], err[128];
    if (!c->show || !number || lj_app_rec_path(LJ_REC_SEQ, number, path, sizeof path) != 0) return -1;
    s = lj_sequence_new_rec();
    if (lj_sequence_load(path, s, err, sizeof err) != 0) { lj_log("sequence %04u: load error: %s", (unsigned)number, err); lj_sequence_free(s); return -1; }
    s->number = number; s->modified = 0;
    old = lj_app_set_current_seq(s);
    lj_sequence_free(old);                                        /* the current sequence is never a table record (lj_app_save_sequence reloads a copy) */
    c->modified[LJ_MOD_SEQ] = 0; c->seq_saved = 1;
    return 0;
}

int lj_app_save_sequence(int as_new)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *cur = c->show ? c->show->current_seq : NULL, *copy, *old;
    const char *why;
    char path[LJ_PATH_CAP], err[128];
    uint32_t number;
    if (!cur) return -1;
    if ((why = lj_sequence_validate(cur)) != NULL) { lj_log("save sequence: %s", why); return -1; }
    number = (as_new || cur->number == 0) ? lj_library_next_number(&lj_a.lib, LJ_REC_SEQ) : cur->number;   /* assigned on first save, immutable [F28] */
    if (!number) { lj_log("save sequence: no number from DirInfo.Ini"); return -1; }
    lj_lock(); cur->number = number; cur->modified = 0; lj_unlock();
    if (lj_app_rec_path(LJ_REC_SEQ, number, path, sizeof path) != 0 || lj_sequence_save(path, cur) != 0) return -1;
    copy = lj_sequence_new_rec();                                 /* the table holds what is on disk; the current record stays the editing copy */
    if (lj_sequence_load(path, copy, err, sizeof err) != 0) { lj_log("sequence %04u: reload after save: %s", (unsigned)number, err); lj_sequence_free(copy); return -1; }
    copy->number = number;
    lj_lock();
    old = (lj_sequence *)lj_table_get(&c->show->sequences, number);
    lj_table_put(&c->show->sequences, number, copy);
    lj_app_repin_cue(0);                                          /* editing a sequence changes every cue [03 s5.1]: every runner on this number gets the new pointer */
    lj_unlock();
    if (old && old != copy) lj_sequence_free(old);
    c->modified[LJ_MOD_SEQ] = 0; c->seq_saved = 1;
    return 0;
}

int lj_app_save_cue(int as_new)
{
    lj_ctx *c = lj_ctx_get();
    lj_cue *cue = c->show ? c->show->current_cue : NULL, *old;
    const char *why;
    char path[LJ_PATH_CAP];
    uint32_t number;
    if (!cue) return -1;
    if ((why = lj_cue_validate(cue)) != NULL) { lj_log("save cue: %s", why); return -1; }
    number = (as_new || cue->number == 0) ? lj_library_next_number(&lj_a.lib, LJ_REC_CUE) : cue->number;
    if (!number) { lj_log("save cue: no number from DirInfo.Ini"); return -1; }
    lj_lock(); cue->number = number; lj_unlock();
    if (lj_app_rec_path(LJ_REC_CUE, number, path, sizeof path) != 0 || lj_cue_save(path, cue) != 0) return -1;
    lj_lock();
    old = (lj_cue *)lj_table_get(&c->show->cues, number);
    if (old != cue) { lj_table_put(&c->show->cues, number, cue); lj_app_repin_cue(0); }   /* the current cue record becomes the table's (and the engine's) */
    lj_unlock();
    if (old && old != cue) lj_cue_free(old);
    c->modified[LJ_MOD_CUE] = 0; c->cue_saved = 1;
    return 0;
}

int lj_app_delete_record(int kind, uint32_t number)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = c->show;
    void *rec = NULL;
    lj_cue *blank = NULL;
    lj_cmd cmd;
    if (!show || !number) return -1;
    memset(&cmd, 0, sizeof cmd);
    cmd.at_us = lj_clock_us();
    switch ((enum lj_rec_kind)kind) {
    case LJ_REC_SEQ:
        lj_lock();
        rec = lj_table_remove(&show->sequences, number);
        if (show->current_cue) lj_app_repin_cue(number);                 /* its slots show load error from now on */
        if (show->current_seq && show->current_seq->number == number) show->current_seq->number = 0;   /* a number is never reused [F28] */
        lj_unlock();
        lj_sequence_free((lj_sequence *)rec);
        break;
    case LJ_REC_CUE:
        blank = lj_cue_new_rec();
        lj_lock();
        rec = lj_table_remove(&show->cues, number);
        if (show->current_cue == rec) {
            show->current_cue = blank; blank = NULL;
            cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 0; cmd.cue = NULL;        /* every runner handed NULL before the record goes (rule 6) */
            if (c->engine) lj_engine_cmd(c->engine, &cmd);
        }
        lj_unlock();
        lj_cue_free((lj_cue *)rec);
        lj_cue_free(blank);
        break;
    case LJ_REC_BG:
        lj_lock();
        rec = lj_table_remove(&show->bgcues, number);
        if (rec) { cmd.kind = LJ_CMD_LOAD_BG; cmd.a = 0; cmd.bg = NULL; if (c->engine) lj_engine_cmd(c->engine, &cmd); }
        lj_unlock();
        lj_bgcue_free((lj_bgcue *)rec);
        break;
    default:                                                          /* cue lists (W3) and macros (W2): file only */
        break;
    }
    return lj_library_delete_record(&lj_a.lib, (enum lj_rec_kind)kind, number);
}

/* ---- lj_fn rows that need the disk: the programs and the quick saves ---- */

static void lj_app_fn(enum lj_fn fn, int edge, uint32_t prog_num, void *ud)
{
    lj_cmd cmd;
    lj_app_fresh fresh;
    (void)ud;
    switch (fn) {
    case LJ_FN_PROG_CUE:   if (edge) lj_app_load_cue(prog_num, 0); break;
    case LJ_FN_PROG_BGCUE: if (edge) lj_app_load_bgcue(prog_num); break;
    case LJ_FN_PROG_SEQ_FLASH:                                        /* held: the pinned sequence flashes; released: NULL */
        memset(&cmd, 0, sizeof cmd); memset(&fresh, 0, sizeof fresh);
        cmd.kind = LJ_CMD_FLASH; cmd.b = (uint32_t)(edge != 0);
        if (edge) cmd.seq[0] = lj_app_seq_for(prog_num, &fresh);
        lj_lock(); lj_app_commit(&fresh, &cmd); lj_unlock();
        break;
    case LJ_FN_QUICK_SAVE_SEQUENCE: if (edge) lj_app_save_sequence(0); break;
    case LJ_FN_QUICK_SAVE_CUE:      if (edge) lj_app_save_cue(0); break;
    case LJ_FN_PROG_CUELIST:        if (edge) lj_log("cue list %u: cue lists are W3", (unsigned)prog_num); break;
    default: break;
    }
}

/* ---- Startup Options, Main.Ini [STARTUP] ([01 s6.2]) ---- */

static void lj_app_startup_options(lj_ini *ini)
{
    lj_cmd cmd;
    int n;
    if (!ini) return;
    if ((n = lj_ini_geti(ini, "STARTUP", "AutoLampOn", 0)) != 0) lj_log("startup: Auto LampOn after %d min is W2 (lamp-on cycle)", n);
    if ((n = lj_ini_geti(ini, "STARTUP", "Seq", 0)) != 0 && lj_app_load_sequence((uint32_t)n) == 0 && lj_ini_geti(ini, "STARTUP", "SeqRun", 0)) {
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = LJ_CMD_GO; cmd.a = LJ_RUN_CURRENT;
        lj_ctx_post(&cmd);
    }
    if ((n = lj_ini_geti(ini, "STARTUP", "Cue", 0)) != 0) lj_app_load_cue((uint32_t)n, 0);
    if ((n = lj_ini_geti(ini, "STARTUP", "PCUE", 0)) != 0) lj_app_load_bgcue((uint32_t)n);   /* PROPOSAL: PCUE = the BG Cue # row of [01 s6.2]; LJ2's key meaning UNVERIFIED */
    if ((n = lj_ini_geti(ini, "STARTUP", "Cl", 0)) != 0) lj_log("startup: cue list %d (ClRun=%d) is W3", n, lj_ini_geti(ini, "STARTUP", "ClRun", 0));
    if (lj_ini_geti(ini, "STARTUP", "ShowNotes", 0)) lj_log("startup: Show Notes on Startup is W2");
    if (lj_ini_geti(ini, "STARTUP", "Minimized", 0) && lj_a.main) {
        struct lj_menu *m = lj_window_menus(lj_a.main);
        if (m && m->host && m->host->sys_cmd) m->host->sys_cmd(m->host->ud, lj_a.main, LJ_SYS_MINIMIZE);   /* Run Minimized through the platform's caption-button path */
    }
}

/* ---- --selftest: the W0 pixel compare plus the W1 startup line ---- */

static int lj_app_selftest(lj_window *seq, int startup_ok)
{
    lj_surface got, ref_bmp;
    const lj_surface *ref;
    int bad = 0, i, n;
    memset(&got, 0, sizeof got); memset(&ref_bmp, 0, sizeof ref_bmp);
    printf("selftest startup: main caption \"%s\" while the library loaded, status window open: %s\n", LJ_STARTUP_PLEASE_WAIT, startup_ok ? "OK" : "FAIL");
    if (!seq) { printf("selftest FAIL: TSEQUENCEFORM did not open\n"); return 1; }
    lj_window_set_active(seq, 0);
    lj_platform_present(seq);
    if (lj_platform_readback(seq, &got) != 0) { printf("selftest FAIL: read-back\n"); return 1; }
    if (lj_a.selftest_ref) {
        if (lj_platform_load_bmp(lj_a.selftest_ref, &ref_bmp) != 0) { printf("selftest FAIL: cannot load %s\n", lj_a.selftest_ref); lj_surface_free(&got); return 1; }
        ref = &ref_bmp;
    } else ref = lj_window_surface(seq);                     /* W1: the live window's own surface (behaviour files change captions and sizes), so the claim is the platform's blit path is pixel-exact */
    if (got.w != ref->w || got.h != ref->h) bad = -1;
    else for (i = 0, n = got.w * got.h; i < n; i++) bad += (got.px[i] & 0xFFFFFFu) != (ref->px[i] & 0xFFFFFFu);
    printf("selftest TSEQUENCEFORM: screen %dx%d vs %s %dx%d, %d mismatching pixel(s): %s\n",
           got.w, got.h, lj_a.selftest_ref ? "ref.bmp" : "surface", ref->w, ref->h, bad, bad == 0 ? "OK" : "FAIL");
    lj_surface_free(&got);
    lj_surface_free(&ref_bmp);
    return (bad == 0 && startup_ok) ? 0 : 1;
}

/* ---- startup and shutdown ([01 s6.2], lj_app.h header comment) ---- */

static lj_ini *lj_app_ini(const char *dir, const char *name, char *path, size_t cap)
{
    lj_ini *ini;
    if (snprintf(path, cap, "%s%s", dir, name) >= (int)cap) { lj_log("app: path too long for %s", name); path[0] = 0; return lj_ini_new(); }
    ini = lj_ini_load(path);
    return ini ? ini : lj_ini_new();
}

static void lj_app_save_ini(lj_ini *ini, const char *path)
{
    if (ini && path[0] && lj_ini_save(ini, path) != 0) lj_log("app: cannot save %s", path);
}

int lj_app_main(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = NULL;
    lj_engine *engine = NULL;
    lj_window *status = NULL, *seq, *cue;
    lj_uiscript_host host;
    char main_path[LJ_PATH_CAP], forms_path[LJ_PATH_CAP], hw_path[LJ_PATH_CAP], hot_path[LJ_PATH_CAP], caption[128], err[128];
    int i, startup_ok = 0, sacn_ok, show_ok;
    lj_cmd cmd;
    lj_widget *root;

    memset(&lj_a, 0, sizeof lj_a);
    lj_a.cfg.tick_ms = 100; lj_a.cfg.zoom = 1;
    for (i = 1; i < argc; i++) {                                                    /* 1. argv */
        if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) lj_a.save_path = argv[++i];
        else if (strcmp(argv[i], "--selftest") == 0) { lj_a.selftest = 1; if (i + 1 < argc && argv[i + 1][0] != '-') lj_a.selftest_ref = argv[++i]; }
        else if (strcmp(argv[i], "--explode") == 0) lj_a.explode = 1;
        else if (strcmp(argv[i], "--ms") == 0 && i + 1 < argc) lj_a.run_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--native") == 0) lj_a.cfg.native_frame = 1;
        else if (strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) lj_a.cfg.zoom = atoi(argv[++i]);
        else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) lj_a.script_path = argv[++i];
        else if (strcmp(argv[i], "--version") == 0) { printf("LJ4EVR %s\n", LJ_VERSION_STR); return 0; }
        else if (strncmp(argv[i], "U:", 2) == 0) lj_a.u_arg = argv[i] + 2;
        else { fprintf(stderr, "lj4evr %s: unknown argument %s\n", LJ_VERSION_STR, argv[i]); return 2; }
    }

    /* 3. pref path (UTF-8 bytes, only concatenated: ME-65) -> library -> the ini files */
    if (lj_platform_pref_path(lj_a.pref, sizeof lj_a.pref) != 0) lj_a.pref[0] = 0;
    lj_startup_library(lj_a.pref, lj_a.u_arg, &lj_a.lib);
    c->main_ini = lj_app_ini(lj_a.lib.root, "Main.Ini", main_path, sizeof main_path);
    c->forms_ini = lj_app_ini(lj_a.lib.root, "forms.ini", forms_path, sizeof forms_path);
    c->hardware_ini = lj_app_ini(lj_a.pref, "Hardware.Ini", hw_path, sizeof hw_path);
    lj_a.cfg.zoom = lj_ini_geti(c->main_ini, "CHROME", "Zoom", lj_a.cfg.zoom);
    lj_a.cfg.native_frame = lj_ini_geti(c->main_ini, "CHROME", "Native", lj_a.cfg.native_frame);
    lj_a.tick_hz = lj_prefs_tick_hz(c->main_ini);                                   /* [ENGINE] TickHz clamped 10..200 (ME-24) */
    lj_startup_last_run(c->main_ini);                                               /* [OTHER] LastRun=LJ4EVR x.y.z (9.4) */
    if (lj_a.selftest) lj_a.cfg.zoom = 1;
    lj_net_startup();

    /* 4. platform, the show, the context */
    if (lj_platform_init(&lj_a.cfg) != 0) { lj_a.rc = 1; goto out; }
    show = lj_show_new();
    show->current_seq = lj_sequence_new_rec();
    if (!show->current_seq->scenes) lj_sequence_alloc(show->current_seq, 1);      /* one all-off scene */
    show->current_cue = lj_cue_new_rec();
    lj_hotkeys_init(&lj_a.hotkeys);
    c->show = show; c->lib = &lj_a.lib; c->sacn = &lj_a.sacn; c->hotkeys = &lj_a.hotkeys;
    lj_str_copy(c->userdir, sizeof c->userdir, lj_a.pref);
    c->current_scene = 0; c->blind = 0;

    /* 5. engine (bound after the show is loaded) */
    engine = lj_engine_create();
    if (engine) lj_engine_set_tick_us(engine, 1000000u / (uint32_t)lj_a.tick_hz);
    c->engine = engine;

    /* 6. main window, "Please Wait.." until the library is open (LORD_FIDELITY-20) */
    lj_a.main = lj_desktop_create();
    lj_a.main_is_desktop = lj_a.main != NULL;
    if (!lj_a.main) lj_a.main = lj_main_window_create(lj_app_main_bindings);       /* ponytail: W0 stub until desktop/ lands */
    if (!lj_a.main || lj_platform_open_window(lj_a.main, LJ_WF_MAIN) != 0) { lj_log("app: no main window"); lj_a.rc = 1; goto out; }
    root = lj_window_root(lj_a.main);
    lj_a.main_on_event = root->on_event; root->on_event = lj_app_main_event;       /* the close prompt wraps the desktop's root handler */
    lj_window_set_caption(lj_a.main, LJ_STARTUP_PLEASE_WAIT);
    lj_platform_present(lj_a.main);
    lj_win_set_host(&lj_app_host);
    status = lj_win_open(LJ_WIN_PLEASE_WAIT);
    if (!status && (lj_a.status_own = lj_startup_status_create()) != NULL && lj_app_host_open(NULL, lj_a.status_own, LJ_WF_STAY_ON_TOP) != 0) { lj_window_destroy(lj_a.status_own); lj_a.status_own = NULL; }
    startup_ok = strcmp(lj_window_caption(lj_a.main)->text, LJ_STARTUP_PLEASE_WAIT) == 0 && (status || lj_a.status_own);

    show_ok = lj_show_load(show, &lj_a.lib) == 0;                                   /* Config.dat -> fixtures, DefaultOut.dat, groups, statics, presets */
    if (!show_ok) lj_log("app: Config.dat under %s did not load (see the lines above)", lj_a.lib.root);
    if (lj_hotkeys_setup_path(lj_a.pref, 1, hot_path, sizeof hot_path) == 0 && lj_hotkeys_load(hot_path, &lj_a.hotkeys, err, sizeof err) != 0) lj_hotkeys_init(&lj_a.hotkeys);
    sacn_ok = lj_prefs_sacn(c->hardware_ini, hw_path, &lj_a.sacn) == 0;            /* CID generated and saved once */
    lj_lock();
    if (engine) {
        lj_engine_bind(engine, show);
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = LJ_CMD_CURRENT_SEQ; cmd.seq[0] = show->current_seq; cmd.at_us = lj_clock_us();
        lj_engine_cmd(engine, &cmd);
        if (sacn_ok) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_SACN_CFG; cmd.sacn = &lj_a.sacn; cmd.at_us = lj_clock_us(); lj_engine_cmd(engine, &cmd); }   /* ME-23: applied by the engine thread */
    }
    lj_unlock();
    if (status) lj_win_close(LJ_WIN_PLEASE_WAIT); else lj_app_close_form(&lj_a.status_own);
    if (lj_win_caption_main(caption, sizeof caption, lj_a.lib.name) == 0) lj_window_set_caption(lj_a.main, caption);

    /* 7. the lj_fn rows that need the disk, the W1 windows, Startup Options */
    lj_fn_bind(LJ_FN_PROG_CUE, lj_app_fn, NULL); lj_fn_bind(LJ_FN_PROG_BGCUE, lj_app_fn, NULL); lj_fn_bind(LJ_FN_PROG_SEQ_FLASH, lj_app_fn, NULL);
    lj_fn_bind(LJ_FN_PROG_CUELIST, lj_app_fn, NULL); lj_fn_bind(LJ_FN_QUICK_SAVE_SEQUENCE, lj_app_fn, NULL); lj_fn_bind(LJ_FN_QUICK_SAVE_CUE, lj_app_fn, NULL);
    seq = lj_win_open(LJ_WIN_SEQ_FORM);
    cue = lj_win_open(LJ_WIN_CUE_FORM);
    if (!seq) seq = lj_a.seq_fb = lj_app_open_form(&lj_form_TSEQUENCEFORM);   /* ponytail: W0 fallback until windows/ lands */
    if (!cue) lj_a.cue_fb = lj_app_open_form(&lj_form_TCURRENTCUE);
    lj_app_startup_options(c->main_ini);
    if (lj_a.save_path && lj_bmp_write(lj_window_surface(lj_a.main), lj_a.save_path) == 0) printf("saved %s\n", lj_a.save_path);
    if (lj_a.selftest) { lj_a.rc = lj_app_selftest(seq, startup_ok); goto out; }

    /* 8. the engine thread and the UI loop */
    if (engine && lj_platform_engine_start(engine, lj_engine_tick_us(engine)) != 0) lj_log("app: engine thread did not start");
    host.ud = NULL; host.resolve = lj_app_resolve; host.view = lj_app_view;
    lj_uiscript_set_host(&host);
    if (lj_a.script_path && lj_uiscript_load(lj_a.script_path) != 0) { lj_a.rc = 1; lj_a.no_save = 1; goto out; }
    lj_a.t0 = lj_a.last_refresh = lj_clock_us();
    lj_platform_run(lj_app_idle, NULL);

out:
    lj_platform_engine_stop();                                     /* ME-23: join the engine thread, then the sender closes (3 x Stream_Terminated per universe) */
    lj_sacn_close();                                               /* no-op when the thread's last act already closed it; the UI thread is the only thread now */
    lj_win_close_all();
    lj_ctl_close_all();
    lj_app_close_form(&lj_a.seq_fb);
    lj_app_close_form(&lj_a.cue_fb);
    lj_app_close_form(&lj_a.status_own);
    if (lj_a.main) {
        lj_platform_close_window(lj_a.main);
        if (lj_a.main_is_desktop) lj_desktop_destroy(lj_a.main); else lj_main_window_destroy(lj_a.main);
        lj_a.main = NULL;
    }
    lj_fn_unbind_all();
    lj_lock();
    if (engine) lj_engine_bind(engine, NULL);
    c->engine = NULL;
    lj_unlock();
    lj_engine_destroy(engine);
    lj_lock(); lj_show_free(show); c->show = NULL; lj_unlock();
    if (!lj_a.no_save) { lj_app_save_ini(c->forms_ini, forms_path); lj_app_save_ini(c->main_ini, main_path); lj_app_save_ini(c->hardware_ini, hw_path); }
    lj_ini_free(c->forms_ini); lj_ini_free(c->main_ini); lj_ini_free(c->hardware_ini);
    c->forms_ini = c->main_ini = c->hardware_ini = NULL;
    lj_platform_shutdown();
    lj_net_cleanup();
    return lj_a.rc;
}
