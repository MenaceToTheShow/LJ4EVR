/* lj_app_cuelist_host.c - see lj_app_cuelist_host.h. The cue list runner's host (docs/w3-contracts.md seam 1), the editor's generic
 * macro (seam 2), the user-profile / shape reloads (seams 2-3), follow spot recall, the W3 desktop actions and (folded
 * from lj_app_cuelist_host.c, ME-173) the actions that open the W4 windows plus the LightCorder idle hook. Every
 * release-then-free post checks the engine took the new pointer first (ME-102, lj_app_post_locked). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen / localtime under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include "app/lj_app_cuelist_host.h"
#include "app/lj_app_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app/lj_app_seam.h"
#include "app/lj_cuelist_run.h"
#include "controls/lj_ctl.h"
#include "controls/lj_ctl_movement.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "engine/lj_shapes_gen.h"
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_profile.h"
#include "io/lj_fn.h"
#include "io/lj_midi.h"
#include "model/lj_cue.h"
#include "model/lj_cuelist.h"
#include "model/lj_followspot.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "model/lj_movemacro.h"
#include "model/lj_show.h"
#include "model/lj_statics.h"
#include "model/lj_table.h"
#include "windows/lj_config_dialogs.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_lightcorder.h"
#include "windows/lj_genmacro_editor.h"
#include "windows/lj_gs2_dialogs.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#define LJ_GS2_FILE_MAX (16u << 20)   /* an import larger than this is not a profile (PROPOSAL bound; a .Udf with bitmaps is < 1 MB) */

/* ponytail: one app per process (as lj_app_seam.c). */
static struct {
    lj_cl_run run;
    lj_cuelist *grave;        /* a list Link Cuelist replaced while the runner stood in it; freed after the step */
    uint32_t steps;
    lj_mtc mtc;
    lj_gdc_state gdc;
    lj_gm_shapes *shapes;     /* the user's Macros.Mac override in force; NULL = the regenerated built-ins */
    lj_bez_set *bez;          /* show->bez: the library's MacroShapes\*.bez, sampled; NULL = none */
} lj_host;


/* ---- the runner's host (lj_cuelist_run.h): every effect of a cue-list line ---- */

static int h_load_cue(void *ud, uint32_t n) { (void)ud; return lj_app_load_cue(n, 0); }
static int h_load_bg(void *ud, uint32_t n) { (void)ud; return lj_app_load_bgcue(n); }
static void h_fn(void *ud, int f) { (void)ud; if (f > 0 && f < LJ_FN_COUNT) lj_fn_run((enum lj_fn)f, 1); }   /* the "on" half only [RN v2.5 b5] */
static void h_clear_cue(void *ud) { (void)ud; lj_fn_run(LJ_FN_NEW_CLEAR_CUE, 1); }

static int h_post(void *ud, const lj_cmd *cmd)
{
    lj_cmd c = *cmd;
    int rc;
    (void)ud;
    if (!c.at_us) c.at_us = lj_clock_us();
    lj_lock(); rc = lj_app_post_locked(&c); lj_unlock();
    return rc;
}

static int h_flash(void *ud, uint32_t seq, int on)   /* Flash Sequence: the table's record pinned in the post's critical section (rule 6) */
{
    lj_show *show = lj_ctx_get()->show;
    lj_cmd c;
    int rc;
    (void)ud;
    if (!show) return -1;
    if (on && !lj_table_get(&show->sequences, seq)) lj_desktop_run_action(LJ_ACT_PIN_SEQ, (int)seq);   /* on disk only: loaded into the table */
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_FLASH; c.b = on != 0; c.at_us = lj_clock_us();
    lj_lock();
    if (on) c.seq[0] = (const lj_sequence *)lj_table_get(&show->sequences, seq);
    rc = on && !c.seq[0] ? -1 : lj_app_post_locked(&c);
    lj_unlock();
    return rc;
}

static int cl_path(uint32_t number, char *out, size_t cap)
{
    if (!lj_ctx_get()->lib) return -1;
    return number ? lj_library_record_path(lj_ctx_get()->lib, LJ_REC_CL, number, out, cap) : lj_library_path(lj_ctx_get()->lib, "CueLists/AutoSave.Cl2", out, cap);
}

static lj_cuelist *cl_read(const char *path, uint32_t number)
{
    lj_cuelist *cl = lj_cuelist_new_rec();
    char err[128];
    if (!cl) return NULL;
    if (lj_cuelist_load(path, cl, err, sizeof err) != 0) { lj_log("cue list %04u: load error: %s", (unsigned)number, err); lj_cuelist_free(cl); return NULL; }
    if (number) cl->number = number;
    return cl;
}

static int h_link(void *ud, uint32_t number, const lj_cuelist **out)   /* Link Cuelist: the run continues in the new list; the old one is freed after this step */
{
    lj_show *show = lj_ctx_get()->show;
    char path[LJ_PATH_CAP];
    lj_cuelist *cl, *old;
    (void)ud;
    if (!show || !number || cl_path(number, path, sizeof path) != 0 || !(cl = cl_read(path, number))) return -1;
    lj_lock(); old = show->current_cl; show->current_cl = cl; lj_unlock();
    lj_cuelist_free(lj_host.grave);       /* an earlier link of this step: the runner has left it */
    lj_host.grave = old;
    lj_ctx_get()->modified[LJ_MOD_CL] = 0;
    *out = cl;
    return 0;
}

/* Execute File: only with Main.Ini [CUELIST2] AllowExecute=1 (ours, default 0, docs/w3-contracts.md); started detached so
 * the UI loop never waits on it. The line goes through the shell (system()): a path with " $ or a backtick, and
 * parameters with " or a backtick, are refused so the quoting holds, but the parameters still reach the shell, so
 * & | ; > and $(...) in them run. That grants nothing AllowExecute=1 does not already grant (a cue list can name any
 * program with any arguments); docs/running.md and SECURITY.md say so (ME-147). */
static int h_exec(void *ud, const char *path, const char *args)
{
    char line[2 * LJ_PATH_CAP];
    int n;
    (void)ud;
    if (!lj_ctx_get()->main_ini || !lj_ini_geti(lj_ctx_get()->main_ini, "CUELIST2", "AllowExecute", 0)) { lj_log("cue list: Execute File %s skipped ([CUELIST2] AllowExecute=0)", path ? path : ""); return -1; }
    if (!path || !*path || strpbrk(path, "\"$`") || (args && strpbrk(args, "\"`"))) { lj_log("cue list: Execute File refused (quote in the path or parameters)"); return -1; }
#ifdef _WIN32
    n = snprintf(line, sizeof line, "start \"\" \"%s\" %s", path, args ? args : "");
#else
    n = snprintf(line, sizeof line, "\"%s\" %s &", path, args ? args : "");
#endif
    if (n < 0 || (size_t)n >= sizeof line) return -1;
    return system(line) == 0 ? 0 : -1;
}

static int h_clock(void *ud, uint32_t *ms_of_day, uint32_t *day)
{
    time_t t = time(NULL);
    const struct tm *tm = localtime(&t);   /* UI thread only */
    (void)ud;
    if (!tm) return -1;
    *ms_of_day = (uint32_t)((tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec) * 1000);
    *day = (uint32_t)(tm->tm_year * 366 + tm->tm_yday);   /* monotonic across a year end, which is all a date-change wait compares */
    return 0;
}

static int h_timecode(void *ud, int src, uint32_t *ms)
{
    uint32_t day;
    if (src == LJ_TC_MIDI) return lj_mtc_time(&lj_host.mtc, lj_clock_us(), ms);
    if (src == LJ_TC_PC_CLOCK) return h_clock(ud, ms, &day);
    if (src == LJ_TC_MP) return lj_app_io_mp_time(ms);               /* W5: the MediaPlayer clip clock (docs/w5-contracts.md seam 7) */
    return -1;   /* PC elapsed is the runner's own; CD / DA have none in LJ4EVR (design 8.5) */
}

static int h_cue_transparent(void *ud, uint32_t n)   /* ME-171 (g): one cue read per candidate, scanning back from the run line */
{
    const lj_library *lib = lj_ctx_get()->lib;
    char path[LJ_PATH_CAP], err[160];
    lj_cue *cue;
    int t = -1;
    (void)ud;
    if (!lib || lj_library_record_path(lib, LJ_REC_CUE, n, path, sizeof path) != 0 || !(cue = lj_cue_new_rec())) return -1;
    if (lj_cue_load(path, cue, err, sizeof err) == 0) t = cue->transparent != 0;
    lj_cue_free(cue);
    return t;
}

static const lj_cl_host lj_cl_host_app = { NULL, h_load_cue, h_post, h_load_bg, h_flash, h_link, h_fn, h_exec, h_clear_cue, h_timecode, h_clock, lj_app_io_cl_media, h_cue_transparent };

/* ---- the lj_app.h W3 block ---- */

void lj_app_cuelist_step(uint64_t now_us)
{
    lj_ini *mi = lj_ctx_get()->main_ini;
    int t;
    lj_host.steps++;
    lj_ctx_get()->cl = &lj_host.run.st;
    if (mi) {                                                        /* the Cuelist window's Preferences (Main.Ini [CUELIST2], PROPOSAL keys) */
        lj_host.run.ignore_first_wait = lj_ini_geti(mi, "CUELIST2", "IgnoreFirstWait", 0) != 0;
        lj_host.run.auto_cue_on_run = lj_ini_geti(mi, "CUELIST2", "AutoCue", 0) != 0;
        lj_host.run.stop_media = lj_ini_geti(mi, "CUELIST2", "StopAudio", 0) != 0;          /* W5 media: M_StopAudio */
        lj_host.run.autoseek_media = lj_ini_geti(mi, "CUELIST2", "AutoSeekAudio", 0) != 0;  /* W5 media: M_AutoCueAudio */
        t = lj_ini_geti(mi, "CUELIST2", "MTCTimeout", LJ_CL_MTC_STOP);
        lj_host.run.mtc_timeout = (uint8_t)(t >= LJ_CL_MTC_STOP && t <= LJ_CL_MTC_IGNORE ? t : LJ_CL_MTC_STOP);
        t = lj_ini_geti(mi, "CUELIST2", "SyncFrames", 0);                                   /* W6 INT-s7-SYNCFRAMES: Off / 1 / 5 / 10 (the window's items) */
        lj_host.mtc.sync_frames = (uint8_t)(t == 1 || t == 5 || t == 10 ? t : 0);
    }
    lj_cl_run_step(&lj_host.run, now_us, &lj_cl_host_app);
    if (lj_host.grave && lj_host.run.cl != lj_host.grave) { lj_cuelist_free(lj_host.grave); lj_host.grave = NULL; }
}

uint32_t lj_app_cuelist_steps(void) { return lj_host.steps; }

void lj_app_cuelist_pass(uint64_t now_us)
{
    lj_app_io_idle(now_us);                                          /* W6 LORD_ENGINE-9: the media clock and the inputs first */
    lj_app_cuelist_step(now_us);                                     /* W3: the cue list runs here, once per 10 ms wake, never in the engine (design 2.3 rule 4) */
    lj_app_cuelist_host_idle(now_us);                                /* W4: the LightCorder recorder samples frames here (UI thread, ME-2) */
}

void lj_app_mtc_feed(uint8_t qf_data) { lj_mtc_feed(&lj_host.mtc, qf_data, lj_clock_us()); }

static void cl_replace(lj_cuelist *cl)   /* the new current list; the runner lets go of the old one before it is freed (rule 6, UI side) */
{
    lj_show *show = lj_ctx_get()->show;
    lj_cuelist *old;
    lj_lock(); old = show->current_cl; show->current_cl = cl; lj_unlock();
    lj_cl_run_unpin(&lj_host.run, old);
    lj_cuelist_free(old);
    lj_ctx_get()->modified[LJ_MOD_CL] = 0;
}

int lj_app_load_cuelist_path(const char *path, uint32_t number)
{
    lj_cuelist *cl;
    if (!lj_ctx_get()->show || !path || !(cl = cl_read(path, number))) return -1;
    cl_replace(cl);
    return 0;
}

int lj_app_load_cuelist(uint32_t number)
{
    char path[LJ_PATH_CAP];
    if (cl_path(number, path, sizeof path) != 0) { lj_log("cue list %04u: no path", (unsigned)number); return -1; }
    return lj_app_load_cuelist_path(path, number);
}

static int cl_op_now(int op, uint32_t n)
{
    lj_show *show = lj_ctx_get()->show;
    return show ? lj_cl_run_op(&lj_host.run, show->current_cl, op, n, lj_clock_us(), &lj_cl_host_app) : -1;
}
static int cl_held_op;
static uint32_t cl_held_n;
static void cl_link_yes(int yes, void *ud) { (void)ud; if (yes) cl_op_now(cl_held_op, cl_held_n); }
/* a run started on a modified list with a Link Cuelist line asks first (12 Q9, exe 0x17DF50; the trigger is PROPOSAL rig 12 Q9: the
 * link replaces the list, so unsaved edits would be dropped) */
static int cl_op(int op, uint32_t n)
{
    lj_ctx *c = lj_ctx_get();
    const lj_cuelist *cl = c->show ? c->show->current_cl : NULL;
    uint32_t i;
    int starts = op == LJ_CL_OP_TOP || op == LJ_CL_OP_RUN || op == LJ_CL_OP_MARK_RUN || (op == LJ_CL_OP_GO && lj_host.run.st.state == LJ_CL_IDLE);
    if (starts && cl && c->modified[LJ_MOD_CL] && lj_host.run.st.state == LJ_CL_IDLE)
        for (i = 0; i < cl->n; i++) if (cl->lines[i].cmd == LJ_CL_LINK_CUELIST) {
            cl_held_op = op; cl_held_n = n;
            lj_win_confirm(NULL, "Modified cuelist includes link to other cuelist - run without saving first ?", "", cl_link_yes, NULL);
            return 0;
        }
    return cl_op_now(op, n);
}
static void cl_new_yes(int yes, void *ud) { lj_cuelist *cl; (void)ud; if (yes && lj_ctx_get()->show && (cl = lj_cuelist_new_rec()) != NULL) cl_replace(cl); }

void lj_app_cuelist_prog(uint32_t number) { if (lj_app_load_cuelist(number) == 0) cl_op(LJ_CL_OP_TOP, 0); }

static int cl_save(void)   /* Quick Save CueList: the number is assigned on first save [F28] */
{
    lj_ctx *c = lj_ctx_get();
    lj_cuelist *cl = c->show ? c->show->current_cl : NULL;
    const char *why;
    char path[LJ_PATH_CAP];
    uint32_t n;
    if (!cl || !lj_ctx_get()->lib) return -1;
    if ((why = lj_cuelist_validate(cl)) != NULL) { lj_log("save cue list: %s", why); return -1; }
    if (!cl->number && (n = lj_library_next_number(lj_ctx_get()->lib, LJ_REC_CL)) != 0) { lj_lock(); cl->number = n; lj_unlock(); }
    if (!cl->number || cl_path(cl->number, path, sizeof path) != 0 || lj_cuelist_save(path, cl) != 0) { lj_log("save cue list: not written"); return -1; }
    lj_ctx_set_modified(LJ_MOD_CL, 0);
    return 0;
}

int lj_app_gs2_reload(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_profile *np = NULL, *op;
    int nn = 0, on, i;
    unsigned reopen;
    if (!c->show || !lj_ctx_get()->lib || lj_show_gs2_load_all(lj_ctx_get()->lib, &np, &nn) != 0) return -1;   /* parsed outside the lock */
    lj_lock();
    op = c->show->gs2; on = c->show->ngs2;
    c->show->gs2 = np; c->show->ngs2 = nn;
    for (i = 0; c->engine && i < LJ_FIXTURES; i++)                 /* every GS2 fixture re-bound before the old array goes (rule 6, ME-31) */
        if (c->show->fixture[i].kind == LJ_FX_GS2 && c->show->fixture[i].link) lj_engine_bind_profile(c->engine, i, lj_show_gs2(c->show, c->show->fixture[i].gs2_file));
    lj_unlock();
    reopen = lj_ctl_drop_profiles(op, on);                           /* ME-110: no control or editor keeps a pointer into the old array */
    lj_genmacro_editor_reprofile();
    lj_show_gs2_free(op, on);
    for (i = 0; i < LJ_CTL_COUNT; i++) if (reopen >> i & 1u) lj_ctl_open((enum lj_ctl_kind)i);   /* rebuilt from the new array */
    return 0;
}

int lj_app_gm_shapes_reload(void)   /* ME-34: <library>\Override\Macros.Mac (+ UserMacros.Mac) when present, else the built-ins */
{
    lj_ctx *c = lj_ctx_get();
    char mac[LJ_PATH_CAP], user[LJ_PATH_CAP], err[128];
    lj_gm_shapes *sh = NULL, *old = NULL;
    lj_cmd cmd;
    FILE *f;
    int rc;
    if (lj_ctx_get()->lib && lj_library_path(lj_ctx_get()->lib, LJ_GM_OVERRIDE_DIR "/Macros.Mac", mac, sizeof mac) == 0 && (f = fopen(mac, "rb")) != NULL) {
        fclose(f);
        if (lj_library_path(lj_ctx_get()->lib, LJ_GM_OVERRIDE_DIR "/UserMacros.Mac", user, sizeof user) != 0) user[0] = 0;
        if ((sh = (lj_gm_shapes *)calloc(1, sizeof *sh)) != NULL && lj_gm_shapes_load(mac, user[0] ? user : NULL, sh, err, sizeof err) != 0) {
            lj_log("generic macro shapes: %s: %s, the built-ins are used", mac, err);
            free(sh); sh = NULL;
        }
    }
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_GM_SHAPES; cmd.gm_shapes = sh; cmd.at_us = lj_clock_us();
    lj_lock();
    if ((rc = lj_app_post_locked(&cmd)) == 0) { old = lj_host.shapes; lj_host.shapes = sh; c->gm_shapes = sh ? sh : &lj_gm_shapes_builtin; }
    lj_unlock();
    if (rc != 0) old = sh;                                           /* never handed over: ours to free; the old table stays in force */
    if (old) { lj_gm_shapes_free(old); free(old); }
    return rc;
}

int lj_app_bez_reload(void)   /* MacroShapes\*.bez parsed outside the lock, show->bez swapped under it, the old set freed after */
{
    lj_ctx *c = lj_ctx_get();
    char dir[LJ_PATH_CAP];
    lj_bez_set *ns, *old;
    if (!c->show || !lj_ctx_get()->lib || lj_library_path(lj_ctx_get()->lib, LJ_BEZ_DIR, dir, sizeof dir) != 0) return -1;
    if (!(ns = (lj_bez_set *)calloc(1, sizeof *ns))) return -1;
    if (lj_bez_set_load(dir, ns) != 0) { free(ns); return -1; }
    lj_lock();
    old = lj_host.bez; lj_host.bez = ns; c->show->bez = ns;
    lj_unlock();
    if (old) { lj_bez_set_free(old); free(old); }
    return 0;
}

int lj_app_followspot(int setting)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = c->show;
    const lj_followspot *fs;
    uint8_t sel[LJ_FIXTURES];
    lj_cmd cmd;
    int n, i, first = 1, g;
    if (!show) return -1;
    if (setting == 0) {                                              /* Clear Followspot: an auto-clear static goes with it */
        fs = c->followspot ? &show->followspot[c->followspot - 1] : NULL;
        if (fs && fs->static_auto_clear && (g = lj_static_global_index(fs->static_page, fs->static_slot)) >= 0) {
            memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_STATIC_DEACTIVATE; cmd.a = (uint32_t)g; lj_ctx_post(&cmd);
        }
        c->followspot = 0;
        return 0;
    }
    if (setting < 1 || setting > LJ_FOLLOWSPOT || !show->current_seq || c->current_scene >= show->current_seq->n) return -1;
    fs = &show->followspot[setting - 1];
    lj_lock(); n = lj_followspot_apply(fs, show, &show->current_seq->scenes[c->current_scene], NULL, sel); lj_unlock();   /* ponytail: "Use only selected fixtures" not read (only = NULL) */
    if (n < 0) { lj_log("follow spot %d: not programmed", setting); return -1; }
    lj_sel_none();
    for (i = 0; i < LJ_FIXTURES; i++) if (sel[i]) { lj_sel_set(i, 1, first); first = 0; }
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    lj_ctx_set_scene(c->current_scene);                              /* posts CURRENT_SEQ: the scene was written under the lock (seam 5) */
    if (fs->static_page >= 1 && fs->static_page <= show->npages && fs->static_slot >= 1 && fs->static_slot <= show->pages[fs->static_page - 1].n) {
        const lj_static_slot *st = &show->pages[fs->static_page - 1].slot[fs->static_slot - 1];
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = LJ_CMD_STATIC_ACTIVATE; cmd.a = (uint32_t)lj_static_global_index(fs->static_page, fs->static_slot); cmd.b = 0xFFFFu; cmd.c = 0xFFu; cmd.st = st; cmd.at_us = lj_clock_us();
        lj_lock();
        if (st->kind == LJ_STATIC_LINK) cmd.seq[0] = (const lj_sequence *)lj_table_get(&show->sequences, st->link_seq);
        (void)lj_app_post_locked(&cmd);
        lj_unlock();
    }
    lj_ctl_open(LJ_CTL_INTENSITY);                                   /* activation opens Intensity + Movement [04 s13] */
    lj_ctl_open(LJ_CTL_MOVEMENT);
    c->followspot = (uint8_t)setting;
    return 0;
}

static int gs2_ieq_base(const char *a, const char *b)   /* "Name.GS2" vs "Name": equal up to the extension, case-insensitive */
{
    for (; *a && *b && *a != '.' && *b != '.'; a++, b++) if ((*a | 0x20) != (*b | 0x20)) return 0;
    return (!*a || *a == '.') && (!*b || *b == '.');
}

int lj_app_gs2_import(const char *path, char *err, size_t cap)
{
    lj_ctx *c = lj_ctx_get();
    const char *base, *dot;
    char name[64], dst[LJ_PATH_CAP], rel[96];
    uint8_t *buf = NULL;
    lj_profile *p = NULL;
    long len = 0;
    FILE *f;
    int rc = -1, udf, i;
    if (err && cap) err[0] = 0;
    if (!path || !c->show || !lj_ctx_get()->lib) { snprintf(err, cap, "no library"); return -1; }
    base = path + strlen(path);
    while (base > path && base[-1] != '/' && base[-1] != '\\') base--;
    dot = strrchr(base, '.');
    udf = dot && (dot[1] | 0x20) == 'u' && (dot[2] | 0x20) == 'd' && ((dot[3] | 0x20) == 'f' || (dot[3] | 0x20) == 'o') && !dot[4];   /* .Udf / .Udo, any case */
    if (snprintf(name, sizeof name, "%.*s.GS2", (int)(dot ? (size_t)(dot - base) : strlen(base)), base) >= (int)sizeof name || name[0] == '.') { snprintf(err, cap, "file name too long"); return -1; }
    if (!(f = fopen(path, "rb"))) { snprintf(err, cap, "cannot read %s", path); return -1; }
    if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && (unsigned long)len <= LJ_GS2_FILE_MAX && fseek(f, 0, SEEK_SET) == 0 &&
        (buf = (uint8_t *)malloc((size_t)len)) != NULL && fread(buf, 1, (size_t)len, f) == (size_t)len && (p = (lj_profile *)calloc(1, sizeof *p)) != NULL)
        rc = udf ? lj_udf_parse(buf, (size_t)len, p, err, cap) : lj_gs2_read(buf, (size_t)len, p, err, cap);
    else snprintf(err, cap, "cannot read %s (%ld bytes)", path, len);
    fclose(f);
    free(buf);
    if (rc == 0) {
        snprintf(rel, sizeof rel, "UserFixtureDef/%s", name);
        if (lj_library_path(lj_ctx_get()->lib, rel, dst, sizeof dst) != 0 || lj_gs2_save(dst, p) != 0) { snprintf(err, cap, "cannot write %s", rel); rc = -1; }   /* PROPOSAL rig: LJ2's import error box text not captured (LORD_FIDELITY-14) */
    }
    if (p) { lj_gs2_free(p); free(p); }
    if (rc != 0) return -1;
    lj_app_gs2_reload();
    for (i = 0; i < c->show->ngs2; i++) if (gs2_ieq_base(c->show->gs2[i].gs2_file, name)) return 0;
    snprintf(err, cap, "%s written but not listed after Reload profile data", name);   /* PROPOSAL rig (LORD_FIDELITY-14) */
    return -1;
}

/* ---- the editor's generic macro (seam 2): ctx->gm_edit is the working record, rank 6 as LJ_GM_SRC_EDITOR ---- */

static int gm_editor_post(lj_genmacro *gm)   /* gm replaces ctx->gm_edit (NULL = re-post the current one) */
{
    lj_ctx *c = lj_ctx_get();
    lj_genmacro *old = NULL;
    lj_cmd cmd;
    int rc;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_GENMACRO; cmd.c = LJ_GM_SRC_EDITOR; cmd.at_us = lj_clock_us();
    lj_lock();
    if (gm) { old = c->gm_edit; c->gm_edit = gm; }
    cmd.gm = c->gm_edit; cmd.a = c->gm_edit ? c->gm_edit->number : 0;
    rc = lj_app_post_locked(&cmd);
    lj_unlock();
    if (rc == 0 && old && old != gm) free(old);                      /* ME-102: kept while the engine may still point at it */
    return rc;
}

static int gm_editor_load(uint32_t number)
{
    char path[LJ_PATH_CAP], err[128];
    lj_genmacro *gm;
    if (!number || !lj_ctx_get()->lib || lj_library_record_path(lj_ctx_get()->lib, LJ_REC_MACRO, number, path, sizeof path) != 0 || !(gm = lj_genmacro_new_rec())) return -1;
    if (lj_genmacro_load(path, gm, err, sizeof err) != 0) { lj_log("generic macro %04u: load error: %s", (unsigned)number, err); free(gm); return -1; }
    gm->number = number;
    return gm_editor_post(gm);
}

/* ---- W3 desktop actions (lj_desktop.h, the W3 rows) ---- */

static void act(enum lj_desktop_action a, int arg, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    char rel[64], path[LJ_PATH_CAP];
    (void)ud;
    switch (a) {
    case LJ_ACT_LOAD_CUELIST:    lj_app_load_cuelist((uint32_t)(arg < 0 ? 0 : arg)); break;
    case LJ_ACT_CUELIST_OP:      cl_op(LJ_CL_OP_OF(arg), LJ_CL_OP_N(arg)); break;
    case LJ_ACT_NEW_CUELIST:                                          /* toolbar, Cuelist window and File menu (12 Q9: exe 0x188D9C / 0x17FD7C; the pairing PROPOSAL rig 12 Q9) */
        if (c->modified[LJ_MOD_CL] && (!c->prefs || c->prefs->warn_cl_modified)) lj_win_confirm(NULL, "Current CueList was modified", "This will clear the current cuelist", cl_new_yes, NULL);
        else cl_new_yes(1, NULL);
        break;
    case LJ_ACT_SAVE_CUELIST:    lj_win_open(LJ_WIN_SAVE_CUELIST); break;
    case LJ_ACT_LOAD_GENMACRO:   gm_editor_load((uint32_t)arg); break;
    case LJ_ACT_GENMACRO_APPLY:  gm_editor_post(NULL); break;
    case LJ_ACT_GENMACRO_TO_CUE:                                      /* Add Macro to Cue: the working cue's gen_macro, pinned as the cue's source */
        if (!c->show || !c->show->current_cue || arg <= 0) break;
        lj_lock(); c->show->current_cue->gen_macro = (uint32_t)arg; lj_unlock();
        lj_ctx_set_modified(LJ_MOD_CUE, 1);
        lj_app_load_genmacro((uint32_t)arg);
        break;
    case LJ_ACT_GM_SHAPES_RELOAD: lj_app_gm_shapes_reload(); break;
    case LJ_ACT_BEZ_RELOAD:      lj_app_bez_reload(); break;
    case LJ_ACT_GS2_RELOAD:      lj_app_gs2_reload(); break;
    case LJ_ACT_GS2_EXPORT_ALL:  lj_gs2_export_all_open(); break;    /* TExportAllForm: Export\ of the library (lj_gs2_dialogs.c) */
    case LJ_ACT_FOLLOWSPOT_APPLY: lj_app_followspot(arg); break;
    case LJ_ACT_FOLLOWSPOT:                                           /* tb_fixture_18: left = re-activate the last setting, right = the list [04 s13] */
        if (arg == 0 && c->followspot) lj_app_followspot(c->followspot); else lj_win_open(LJ_WIN_FOLLOWSPOT_LIST);
        break;
    case LJ_ACT_FOLLOWSPOT_SETTINGS: lj_win_open(LJ_WIN_FOLLOWSPOT_FORM); break;
    case LJ_ACT_CUSTOMIZE_FIXTURE:                                    /* icon popup: Customize Fixture (built-in) | Define Fixture Profile (user-definable) [01 s3.6] */
        lj_win_open(c->show && arg >= 0 && arg < LJ_FIXTURES && c->show->fixture[arg].kind == LJ_FX_GS2 ? LJ_WIN_PROFILE_EDITOR : LJ_WIN_CUSTOMIZE_FIXTURE);
        break;
    case LJ_ACT_FIXTURE_APPLY:                                        /* Customize Fixture / Channel Configuration Ok */
        if (!c->show || !lj_ctx_get()->lib || arg < 0 || arg >= LJ_FIXTURES) break;
        if (lj_show_save_config(c->show, lj_ctx_get()->lib) != 0) lj_log("app: Config.dat not saved");
        snprintf(rel, sizeof rel, "UserFixtureDef/Setup.%d.pal", arg + 1);
        if (lj_library_path(lj_ctx_get()->lib, rel, path, sizeof path) != 0 || lj_fixture_custom_save(path, c->show->fixture[arg].custom) != 0) lj_log("app: %s not saved", rel);
        lj_lock(); if (c->engine) lj_engine_rebind(c->engine); lj_unlock();
        break;
    default: break;
    }
}

/* ---- the cue-list hotkey rows (01 s7, lj_app.h W3 block); bound only where no window file bound them ---- */

static void fn_row(enum lj_fn f, int edge, uint32_t prog, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)prog; (void)ud;
    if (!edge) return;
    if (f == LJ_FN_CUELIST_START_TOP) cl_op(LJ_CL_OP_TOP, 0);
    else if (f == LJ_FN_CUELIST_GO) cl_op(LJ_CL_OP_GO, 0);           /* idle = Run from top (03 s6.5) */
    else if (f == LJ_FN_CUELIST_BACK) cl_op(LJ_CL_OP_BACK, 0);
    else if (f == LJ_FN_CUELIST_STOP) cl_op(LJ_CL_OP_STOP, 0);
    else if (f == LJ_FN_CUELIST_STOP_AND_CLEAR) cl_op(LJ_CL_OP_STOP_CLEAR, 0);
    else if (f == LJ_FN_LOG_TIMECODE) cl_op(LJ_CL_OP_LOG, 0);
    else if (f >= LJ_FN_CUELIST_GOTO_MARK_1 && f <= LJ_FN_CUELIST_GOTO_MARK_4) cl_op(LJ_CL_OP_MARK_GOTO, (uint32_t)(f - LJ_FN_CUELIST_GOTO_MARK_1 + 1));
    else if (f >= LJ_FN_CUELIST_RUN_FROM_MARK_1 && f <= LJ_FN_CUELIST_RUN_FROM_MARK_4) cl_op(LJ_CL_OP_MARK_RUN, (uint32_t)(f - LJ_FN_CUELIST_RUN_FROM_MARK_1 + 1));
    else if (f == LJ_FN_QUICK_SAVE_CUELIST) { if (c->prefs && !c->prefs->quick_cl) lj_win_open(LJ_WIN_SAVE_CUELIST); else cl_save(); }   /* [PREFS] QuickCl */
    else if (f >= LJ_FN_FOLLOW_SPOT_1 && f <= LJ_FN_FOLLOW_SPOT_10) lj_app_followspot(f - LJ_FN_FOLLOW_SPOT_1 + 1);
    else if (f == LJ_FN_CLEAR_FOLLOWSPOT) lj_app_followspot(0);
    else if (f == LJ_FN_HOME_SELECTED_FIXTURES) lj_ctl_movement_home_selected();   /* unbound before W3 acceptance ("fn: no binding" from Fixture toolbar button 9) */
}

void lj_app_cuelist_host_start(void)
{
    static const enum lj_desktop_action acts[] = {
        LJ_ACT_LOAD_CUELIST, LJ_ACT_CUELIST_OP, LJ_ACT_NEW_CUELIST, LJ_ACT_SAVE_CUELIST, LJ_ACT_LOAD_GENMACRO, LJ_ACT_GENMACRO_APPLY, LJ_ACT_GENMACRO_TO_CUE,
        LJ_ACT_GM_SHAPES_RELOAD, LJ_ACT_BEZ_RELOAD, LJ_ACT_GS2_RELOAD, LJ_ACT_GS2_EXPORT_ALL, LJ_ACT_FOLLOWSPOT_APPLY, LJ_ACT_FOLLOWSPOT,
        LJ_ACT_FOLLOWSPOT_SETTINGS, LJ_ACT_CUSTOMIZE_FIXTURE, LJ_ACT_FIXTURE_APPLY };
    static const enum lj_fn fns[] = {
        LJ_FN_CUELIST_START_TOP, LJ_FN_CUELIST_GO, LJ_FN_CUELIST_BACK, LJ_FN_CUELIST_STOP, LJ_FN_CUELIST_STOP_AND_CLEAR, LJ_FN_LOG_TIMECODE,
        LJ_FN_CUELIST_GOTO_MARK_1, LJ_FN_CUELIST_GOTO_MARK_2, LJ_FN_CUELIST_GOTO_MARK_3, LJ_FN_CUELIST_GOTO_MARK_4,
        LJ_FN_CUELIST_RUN_FROM_MARK_1, LJ_FN_CUELIST_RUN_FROM_MARK_2, LJ_FN_CUELIST_RUN_FROM_MARK_3, LJ_FN_CUELIST_RUN_FROM_MARK_4,
        LJ_FN_QUICK_SAVE_CUELIST, LJ_FN_CLEAR_FOLLOWSPOT, LJ_FN_HOME_SELECTED_FIXTURES };
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[128];
    int i;
    lj_cl_run_init(&lj_host.run);
    lj_mtc_reset(&lj_host.mtc);
    c->cl = &lj_host.run.st;
    c->gdc = &lj_host.gdc;
    c->gm_shapes = &lj_gm_shapes_builtin;
    if (!c->gm_edit) c->gm_edit = lj_genmacro_new_rec();             /* the editor's blank record; posted on its first load / apply */
    if (c->show && lj_ctx_get()->lib) {
        if (!c->show->current_cl) c->show->current_cl = lj_cuelist_new_rec();   /* the unsaved working list the Cuelist window edits */
        if (lj_library_path(lj_ctx_get()->lib, LJ_FOLLOWSPOT_FILE, path, sizeof path) == 0 && lj_followspot_load(path, c->show->followspot, err, sizeof err) != 0) lj_log("%s: %s", LJ_FOLLOWSPOT_FILE, err);
        lj_app_gs2_reload();
        lj_app_gm_shapes_reload();
        lj_app_bez_reload();
    }
    for (i = 0; i < LJ_COUNT_OF(acts); i++) lj_desktop_bind_action(acts[i], act, NULL);
    for (i = 0; i < LJ_COUNT_OF(fns); i++) if (!lj_fn_bound(fns[i])) lj_fn_bind(fns[i], fn_row, NULL);
    for (i = LJ_FN_FOLLOW_SPOT_1; i <= LJ_FN_FOLLOW_SPOT_10; i++) if (!lj_fn_bound((enum lj_fn)i)) lj_fn_bind((enum lj_fn)i, fn_row, NULL);
}

void lj_app_cuelist_host_stop(void)
{
    lj_ctx *c = lj_ctx_get();
    free(c->gm_edit); c->gm_edit = NULL;                              /* the engine is gone (or unbound) by now */
    if (lj_host.shapes) { lj_gm_shapes_free(lj_host.shapes); free(lj_host.shapes); lj_host.shapes = NULL; }
    c->gm_shapes = &lj_gm_shapes_builtin;
    if (lj_host.bez) { if (c->show && c->show->bez == lj_host.bez) c->show->bez = NULL; lj_bez_set_free(lj_host.bez); free(lj_host.bez); lj_host.bez = NULL; }
    if (lj_host.grave) { lj_cuelist_free(lj_host.grave); lj_host.grave = NULL; }
    lj_cl_run_init(&lj_host.run);
    c->cl = NULL;
}

/* ---- the W4 window actions (was lj_app_cuelist_host.c, docs/w4-contracts.md section 4) ---- */

static void win_act(enum lj_desktop_action a, int arg, void *ud)
{
    (void)ud;
    switch (a) {
    case LJ_ACT_FIXTURE_INFO:      lj_config_dlg_target(arg); lj_win_open(LJ_WIN_FIXTURE_INFO); break;
    case LJ_ACT_FIXTURE_NOTES:     lj_config_dlg_target(arg); lj_win_open(LJ_WIN_FIXTURE_NOTES); break;
    case LJ_ACT_AUTO_ARRANGE:      lj_win_open(LJ_WIN_AUTO_ARRANGE); break;
    case LJ_ACT_DESKTOP_GRID:      lj_win_open(LJ_WIN_DESKTOP_GRID); break;
    case LJ_ACT_DESKTOP_IMAGE:     lj_win_open(LJ_WIN_DESKTOP_IMAGE); break;
    case LJ_ACT_DESKTOP_TABS:      lj_win_open(LJ_WIN_DESKTOP_TABS); break;
    case LJ_ACT_ICON_STYLE:        lj_win_open(LJ_WIN_ICON_STYLE); break;
    case LJ_ACT_NEXTPREV_SETTINGS: lj_win_open(LJ_WIN_NEXTPREV_SELECTION); break;
    case LJ_ACT_RESTORE_DATA:      lj_win_open(LJ_WIN_RESTORE_DATA); break;
    case LJ_ACT_HELP:              lj_help_viewer_topic(NULL); break;
    case LJ_ACT_SELECT_HELPFILE:   if (arg != 1) lj_log("help: only the English topics ship (design 13 item 12)"); lj_help_viewer_topic(NULL); break;
    case LJ_ACT_WINDOWS_MEMORY:    lj_win_open(LJ_WIN_MEMORY_VIEW); break;
    case LJ_ACT_DEBUG_WINDOW:      lj_win_open(LJ_WIN_DEBUG); break;
    case LJ_ACT_FINGERS_SETUP:     lj_win_open(LJ_WIN_FINGERS_SETUP); break;
    case LJ_ACT_DMX_IN:            lj_win_open(LJ_WIN_DMX_IN); break;
    default: break;
    }
}

void lj_app_cuelist_host_windows_start(void)
{
    static const enum lj_desktop_action acts[] = { LJ_ACT_FIXTURE_INFO, LJ_ACT_FIXTURE_NOTES, LJ_ACT_AUTO_ARRANGE, LJ_ACT_DESKTOP_GRID,
        LJ_ACT_DESKTOP_IMAGE, LJ_ACT_DESKTOP_TABS, LJ_ACT_ICON_STYLE, LJ_ACT_NEXTPREV_SETTINGS, LJ_ACT_RESTORE_DATA, LJ_ACT_HELP,
        LJ_ACT_SELECT_HELPFILE, LJ_ACT_WINDOWS_MEMORY, LJ_ACT_DEBUG_WINDOW, LJ_ACT_FINGERS_SETUP, LJ_ACT_DMX_IN };
    size_t i;
    for (i = 0; i < sizeof acts / sizeof acts[0]; i++) lj_desktop_bind_action(acts[i], win_act, NULL);
}

void lj_app_cuelist_host_idle(uint64_t now_us) { lj_lightcorder_step(now_us); }
