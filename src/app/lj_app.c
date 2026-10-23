/* lj_app.c - lj4evr startup [01 s6.2], the UI loop's idle pass, the record-loading
 * seam (design 2.3 rules 2, 3, 6), the 100 ms refresh, the close prompt and the
 * shutdown order (ME-23). The only app file that includes platform/. */
#include "app/lj_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_app_seam.h"
#include "app/lj_app_w3.h"
#include "app/lj_app_w4.h"
#include "app/lj_prefs.h"
#include "app/lj_startup.h"
#include "app/lj_sysmem.h"
#include "app/lj_uiscript.h"
#include "controls/lj_ctl.h"
#include "controls/lj_ctl_common.h"   /* lj_ctl_set_host */
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "gfx/lj_bmp.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"
#include "io/lj_midi.h"
#include "io/lj_net.h"
#include "io/lj_sacn.h"
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
#include "windows/lj_cue_form.h"
#include "windows/lj_prefs.h"
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
    lj_frame frame;                         /* the 100 ms copy */
    int explode, run_ms, selftest, tick_hz, rc, no_save, closing;
    const char *save_path, *selftest_ref, *script_path, *u_arg;
    uint64_t t0, last_refresh, last_mem;
    void (*main_on_event)(lj_widget *w, int ev, void *ud);   /* the desktop's root handler, wrapped for the close prompt */
} lj_a;

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

static void lj_app_host_move(void *ud, lj_window *w, int x, int y) { (void)ud; lj_platform_move_window(w, x, y); }
static int  lj_app_host_pos(void *ud, const lj_window *w, int *x, int *y) { (void)ud; return lj_platform_window_pos(w, x, y); }
static const lj_win_host lj_app_host = { NULL, lj_app_host_open, lj_app_host_close, lj_app_host_show, lj_app_host_move, lj_app_host_pos };   /* move/pos: the warning boxes centre on the main window (LORD_FIDELITY-12) */
static int  lj_app_desktop_open(void *ud, lj_window *w, int stay_on_top) { return lj_app_host_open(ud, w, stay_on_top ? LJ_WF_STAY_ON_TOP : 0u); }
static const lj_desktop_host lj_app_desktop_host = { NULL, lj_app_desktop_open, lj_app_host_close };   /* the Unassigned Fixtures list (W1 acceptance 2: it was built headless without this) */

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
        if (lj_ctx_get()->prefs && !lj_ctx_get()->prefs->confirm_close) { lj_app_quit(); return; }   /* System Preferences > Warnings: CloseConfirm */
        if (lj_a.closing) return;
        lj_a.closing = 1;
        lj_win_close_prompt(lj_app_on_close_result, NULL);
        return;
    }
    if (lj_a.main_on_event) lj_a.main_on_event(w, ev, ud);
}

/* ---- the idle pass and the 100 ms hook ---- */

void lj_app_quit(void) { lj_platform_quit(); }

static const lj_engine_status *lj_app_view(void *ud) { (void)ud; return &lj_ctx_get()->view; }

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
    {   const lj_sysprefs *p = c->prefs;                          /* System Preferences > Warnings gates the red boxes */
        lj_win_warning(LJ_WARN_BLACKOUT, c->view.blackout && (!p || p->warn_blackout));
        lj_win_warning(LJ_WARN_FROZEN, c->view.freeze && (!p || p->warn_freeze));
        lj_win_warning(LJ_WARN_FADE_OUT, c->view.gm_fade == 1);
        lj_win_warning(LJ_WARN_FADE_IN, c->view.gm_fade == 2);
        lj_win_warning(LJ_WARN_OFFLINE, c->view.offline && (!p || p->warn_offline)); }
    if (lj_clock_us() - lj_a.last_mem >= 1000000u) {               /* ME-87: the OS figure once a second, the desktop paints it */
        uint64_t avail, total;
        lj_a.last_mem = lj_clock_us();
        if (lj_sysmem_query(&avail, &total) == 0) lj_desktop_memory_set(avail, total);
    }
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
    lj_app_midi_dispatch();                                            /* seam 2: MIDI events run on this thread */
    now = lj_clock_us();
    lj_app_cuelist_step(now);                                          /* W3: the cue list runs here, once per 10 ms wake, never in the engine (design 2.3 rule 4) */
    lj_app_w4_idle(now);                                               /* W4: the LightCorder recorder samples frames here (UI thread, ME-2) */
    lj_app_seam_idle(now);
    if (now - lj_a.last_refresh >= 100000u) { lj_a.last_refresh = now; lj_app_refresh(); }   /* the 100 ms cadence [11 E24] */
    if (lj_a.run_ms > 0 && (now - lj_a.t0) / 1000u >= (uint64_t)lj_a.run_ms) lj_app_quit();
}

/* ---- the Cue control's host: loads and re-pins go through the seam (lj_app_seam.c), the windows reach the screen here ---- */

static int  lj_app_cue_load_hook(void *ud, uint32_t number, int edit) { (void)ud; return lj_app_load_cue(number, edit); }
static void lj_app_cue_repin_hook(void *ud) { (void)ud; lj_app_repin_current_cue(); }
static void lj_app_cue_show_hook(void *ud, lj_window *w, int open)   /* Cue control, Save Cue, List of Cues, Master Intensity reach the screen through this (fsStayOnTop from the DFM) */
{
    const lj_form_desc *d = lj_window_desc(w);
    if (open) lj_app_host_open(ud, w, d && d->style == LJ_FS_STAYONTOP ? LJ_WF_STAY_ON_TOP : 0u); else lj_app_host_close(ud, w);
}
static const lj_cue_form_host lj_app_cue_host = { NULL, lj_app_cue_load_hook, lj_app_cue_repin_hook, lj_app_cue_show_hook };   /* W1 acceptance 5: without it "Add Sequence to Current Cue" never reached the engine and the windows-b forms stayed off screen */

/* integrator: the desktop menu / toolbar leaves whose window lives in windows/ (lj_desktop.h enum lj_desktop_action); the rest log once */
static void lj_app_action(enum lj_desktop_action a, int arg, void *ud)
{
    (void)ud;
    switch (a) {
    case LJ_ACT_CLOSE:
        if (lj_uiscript_active() || (lj_ctx_get()->prefs && !lj_ctx_get()->prefs->confirm_close)) lj_app_quit();   /* no prompt: script, or CloseConfirm off */
        else if (!lj_a.closing) { lj_a.closing = 1; lj_win_close_prompt(lj_app_on_close_result, NULL); }
        break;
    case LJ_ACT_FIXTURE_CONFIG:  lj_win_open(LJ_WIN_FIXTURE_CONFIG); break;
    case LJ_ACT_HARDWARE_SETUP:  lj_win_open(LJ_WIN_HARDWARE_SETUP); break;
    case LJ_ACT_STARTUP_OPTIONS: lj_win_open(LJ_WIN_STARTUP_OPTIONS); break;
    case LJ_ACT_SAVE_SEQUENCE:   lj_win_open(LJ_WIN_SAVE_SEQ); break;
    case LJ_ACT_SAVE_CUE:        lj_win_open(LJ_WIN_SAVE_CUE); break;
    case LJ_ACT_CTL_OPEN_ONLY:   lj_ctl_open_only((enum lj_ctl_kind)arg); break;
    case LJ_ACT_RESTART:         lj_log("app: restart requested (Restore / Select User Library); start LJ4EVR again"); lj_app_quit(); break;   /* ponytail: LJ2 relaunches itself; relaunch is W4 */
    default: break;
    }
}

/* ---- Startup Options, Main.Ini [STARTUP] ([01 s6.2]) ---- */

static void lj_app_startup_options(lj_ini *ini)
{
    lj_cmd cmd;
    int n;
    if (!ini) return;
    if ((n = lj_ini_geti(ini, "STARTUP", "Seq", 0)) != 0 && lj_app_load_sequence((uint32_t)n) == 0 && lj_ini_geti(ini, "STARTUP", "SeqRun", 0)) {
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = LJ_CMD_GO; cmd.a = LJ_RUN_CURRENT;
        lj_ctx_post(&cmd);
    }
    if ((n = lj_ini_geti(ini, "STARTUP", "Cue", 0)) != 0) lj_app_load_cue((uint32_t)n, 0);
    if ((n = lj_ini_geti(ini, "STARTUP", "PCUE", 0)) != 0) lj_app_load_bgcue((uint32_t)n);   /* PROPOSAL: PCUE = the BG Cue # row of [01 s6.2]; LJ2's key meaning UNVERIFIED */
    if ((n = lj_ini_geti(ini, "STARTUP", "Cl", 0)) != 0 && lj_app_load_cuelist((uint32_t)n) == 0 && lj_ini_geti(ini, "STARTUP", "ClRun", 0))
        lj_desktop_run_action(LJ_ACT_CUELIST_OP, LJ_CL_OP_ARG(LJ_CL_OP_TOP, 0));   /* W3: the cue list, Run = from the top */
    if (lj_ini_geti(ini, "STARTUP", "ShowNotes", 0)) lj_win_open(LJ_WIN_NOTES);   /* AutoLampOn: lj_app_seam_start schedules Striking Lamps */
    if (lj_ini_geti(ini, "STARTUP", "Minimized", 0) && lj_a.main) {
        struct lj_menu *m = lj_window_menus(lj_a.main);
        if (m && m->host && m->host->sys_cmd) m->host->sys_cmd(m->host->ud, lj_a.main, LJ_SYS_MINIMIZE);   /* Run Minimized through the platform's caption-button path */
    }
}

/* ---- the "Startup Status" window lists every log line while the library loads (LORD_FIDELITY-20) ---- */
static void lj_app_status_sink(void *ud, const char *line) { (void)ud; fprintf(stderr, "%s\n", line); lj_startup_status_line(lj_a.status_own, line); }

/* ---- --selftest: the W0 pixel compare plus the W1 startup line ---- */

static int lj_app_unknown_events;
static void lj_app_count_unknown(void *ud, const char *line) { (void)ud; if (strstr(line, "unknown event")) lj_app_unknown_events++; fprintf(stderr, "%s\n", line); }

static int lj_app_selftest(lj_window *seq, int startup_ok)
{
    lj_surface got, ref_bmp;
    const lj_surface *ref;
    lj_window *mon;
    int bad = 0, i, n;
    memset(&got, 0, sizeof got); memset(&ref_bmp, 0, sizeof ref_bmp);
    printf("selftest startup: main caption \"%s\" while the library loaded, status window open: %s\n", LJ_STARTUP_PLEASE_WAIT, startup_ok ? "OK" : "FAIL");
    lj_app_unknown_events = 0;                                /* ME-74 / ME-16: the DMX Output Monitor's DFM names (OnMouseWheelDown/Up) all resolve */
    lj_log_set_sink(lj_app_count_unknown, NULL);
    mon = lj_win_open(LJ_WIN_DMX_MONITOR);
    lj_log_set_sink(NULL, NULL);
    if (mon) lj_win_close(LJ_WIN_DMX_MONITOR);
    printf("selftest TDMXOUTFORM: %s, %d unresolved DFM event name(s): %s\n", mon ? "opened and closed" : "did not open", lj_app_unknown_events, mon && lj_app_unknown_events == 0 ? "OK" : "FAIL");
    if (!mon || lj_app_unknown_events) startup_ok = 0;
    {   /* W2 acceptance (I2): the OS MIDI backend lists its inputs (WinMM / CoreMIDI) and a note posted like a device's comes back off the UI queue */
        static const uint8_t note[3] = { 0x90, 0x3C, 0x7F };
        uint8_t m[3];
        lj_midi_dev dev[16];
        int ndev = lj_midi_list(dev, 16), ok;
        while (lj_midi_poll(m)) {}
        lj_midi_inject(note);
        ok = ndev >= 0 && lj_midi_poll(m) == 1 && memcmp(m, note, 3) == 0;
        printf("selftest MIDI-IN: %d input device(s) listed, injected 90 3C 7F polled back: %s\n", ndev, ok ? "OK" : "FAIL");
        if (!ok) startup_ok = 0;
    }
    {   /* W3 (ME-2): the cue-list runner is stepped once per idle pass of the UI loop, never by the engine */
        uint32_t s0 = lj_app_cuelist_steps();
        for (i = 0; i < 3; i++) lj_app_idle(NULL);
        printf("selftest cue-list runner: %u step(s) over 3 idle passes: %s\n", (unsigned)(lj_app_cuelist_steps() - s0), lj_app_cuelist_steps() - s0 == 3 ? "OK" : "FAIL");
        if (lj_app_cuelist_steps() - s0 != 3) startup_ok = 0;
    }
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
    lj_window *seq, *cue;
    lj_uiscript_host host;
    char main_path[LJ_PATH_CAP], forms_path[LJ_PATH_CAP], hw_path[LJ_PATH_CAP], caption[128];
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
    c->show_nextprev = 1;                                                       /* allow_freeze / allow_offline / allow_solo: lj_app_prefs_load from Main.Ini (ME-29) */

    /* 5. engine (bound after the show is loaded) */
    engine = lj_engine_create();
    if (engine) lj_engine_set_tick_us(engine, 1000000u / (uint32_t)lj_a.tick_hz);
    c->engine = engine;

    /* 6. main window, "Please Wait.." until the library is open (LORD_FIDELITY-20) */
    lj_a.main = lj_desktop_create();
    if (!lj_a.main || lj_platform_open_window(lj_a.main, LJ_WF_MAIN) != 0) { lj_log("app: no main window"); lj_a.rc = 1; goto out; }
    root = lj_window_root(lj_a.main);
    lj_a.main_on_event = root->on_event; root->on_event = lj_app_main_event;       /* the close prompt wraps the desktop's root handler */
    lj_window_set_caption(lj_a.main, LJ_STARTUP_PLEASE_WAIT);
    lj_platform_present(lj_a.main);
    lj_win_set_host(&lj_app_host);
    lj_desktop_set_host(&lj_app_desktop_host);
    lj_cue_form_set_host(&lj_app_cue_host);
    if ((lj_a.status_own = lj_startup_status_create()) != NULL && lj_app_host_open(NULL, lj_a.status_own, LJ_WF_STAY_ON_TOP) != 0) { lj_window_destroy(lj_a.status_own); lj_a.status_own = NULL; }
    startup_ok = strcmp(lj_window_caption(lj_a.main)->text, LJ_STARTUP_PLEASE_WAIT) == 0 && lj_a.status_own;   /* LORD_FIDELITY-20: "Startup Status" lists the load lines */
    if (lj_a.status_own) { lj_log_set_sink(lj_app_status_sink, NULL); lj_platform_present(lj_a.status_own); }

    show_ok = lj_show_load(show, &lj_a.lib) == 0;                                   /* Config.dat -> fixtures, DefaultOut.dat, groups, statics, presets */
    if (!show_ok) lj_log("app: Config.dat under %s did not load (see the lines above)", lj_a.lib.root);
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
    lj_log_set_sink(NULL, NULL);
    lj_app_close_form(&lj_a.status_own);
    if (lj_win_caption_main(caption, sizeof caption, lj_a.lib.name) == 0) lj_window_set_caption(lj_a.main, caption);

    /* 7. prefs + MIDI + hotkey setup + the lj_fn rows that need the disk + the W2 actions (lj_app_seam.c), the W1 windows, Startup Options */
    lj_app_seam_start();
    lj_app_w3_start();                                               /* W3: cue-list runner, editor record, reloads, the W3 actions (lj_app_w3.c) */
    lj_app_w4_start();                                               /* W4: the actions that open W4 windows (lj_app_w4.c) */
    lj_ctl_set_host(&lj_app_host);                                   /* control windows reach the screen the way windows/ does */
    { static const enum lj_desktop_action acts[] = { LJ_ACT_CLOSE, LJ_ACT_FIXTURE_CONFIG, LJ_ACT_HARDWARE_SETUP, LJ_ACT_STARTUP_OPTIONS, LJ_ACT_SAVE_SEQUENCE, LJ_ACT_SAVE_CUE, LJ_ACT_CTL_OPEN_ONLY, LJ_ACT_RESTART };
      for (i = 0; i < LJ_COUNT_OF(acts); i++) lj_desktop_bind_action(acts[i], lj_app_action, NULL); }
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
    lj_app_seam_stop();                                            /* ME-23: the MIDI port closes before the engine thread stops */
    lj_platform_engine_stop();                                     /* ME-23: join the engine thread, then the sender closes (3 x Stream_Terminated per universe) */
    lj_sacn_close();                                               /* no-op when the thread's last act already closed it; the UI thread is the only thread now */
    lj_win_close_all();
    lj_ctl_close_all();
    lj_app_close_form(&lj_a.seq_fb);
    lj_app_close_form(&lj_a.cue_fb);
    lj_app_close_form(&lj_a.status_own);
    if (lj_a.main) {
        lj_platform_close_window(lj_a.main);
        lj_desktop_destroy(lj_a.main);
        lj_a.main = NULL;
    }
    lj_fn_unbind_all();
    lj_lock();
    if (engine) lj_engine_bind(engine, NULL);
    c->engine = NULL;
    lj_unlock();
    lj_engine_destroy(engine);
    lj_app_w3_stop();                                              /* the editor record and the shape override, now that no engine reads them */
    lj_lock(); lj_show_free(show); c->show = NULL; lj_unlock();
    if (!lj_a.no_save) { lj_app_save_ini(c->forms_ini, forms_path); lj_app_save_ini(c->main_ini, main_path); lj_app_save_ini(c->hardware_ini, hw_path); }
    lj_ini_free(c->forms_ini); lj_ini_free(c->main_ini); lj_ini_free(c->hardware_ini);
    c->forms_ini = c->main_ini = c->hardware_ini = NULL;
    lj_platform_shutdown();
    lj_net_cleanup();
    return lj_a.rc;
}
