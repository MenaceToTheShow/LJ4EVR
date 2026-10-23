/* test_media.c - the runnable checks of the W5 media track (docs/media.md; gates W5-MED-1..5, ME-11):
 *  `test_media`      io/lj_media.c against a fake host that is fed sample counts (never the wall clock): the HH:MM:SS:hh
 *                    clock, the WAV-only and size-cap refusals with their messages, the state machine, and the cue list
 *                    runner with the MP lines live: Load, Play, Wait 00:00:02:00 firing at sample frame 88200 and not at
 *                    88199 (44.1 kHz), pause and seek moving the clock, Stop on list stop, Auto seek on Run, the shown MP
 *                    time code and Log; a .Cl2 with MP lines loading clean and round-tripping byte-unchanged, a missing clip
 *                    and a CD line loading red (ME-11).
 *  `test_media_form` the LJ MediaPlayer window headless (transport enable table, time code, file, recent list, greyed
 *                    video / .mps items) with the golden tests/ui/golden/W5MED_MEDIA_PLAYER.bmp (LJ_GOLDEN_UPDATE=1 rewrites
 *                    it; REVIEWED.tsv), and the Cuelist window's MP rows live: ListPop items, sbMPTime, the MP pages
 *                    writing the line, the Mediaplayer Offset dialog, CD / DA still greyed with the hint. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_cuelist_run.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "io/lj_media.h"
#include "model/lj_cuelist.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/lj_window.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_cuelist_form.h"
#include "windows/lj_media_form.h"
#include "windows/lj_show_dialogs.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- the fake playback host: a 10 s clip at 44.1 kHz; the test moves `pos` by hand ---- */
enum { F_RATE = 44100, F_FRAMES = 10 * F_RATE };
static struct { int loaded, playing, loads, plays, seeks, closes; uint64_t pos; char path[260]; } F;
static int f_load(void *ud, const char *path, uint32_t *rate, uint64_t *frames, char *err, size_t cap)
{
    (void)ud;
    if (strstr(path, "missing")) { lj_str_copy(err, cap, "File not found"); return -1; }
    F.loaded = 1; F.playing = 0; F.pos = 0; F.loads++; lj_str_copy(F.path, sizeof F.path, path);
    *rate = F_RATE; *frames = F_FRAMES;
    return 0;
}
static int f_play(void *ud, int on) { (void)ud; F.playing = on; F.plays++; return 0; }
static int f_seek(void *ud, uint64_t frame) { (void)ud; F.pos = frame; F.seeks++; return 0; }
static void f_close(void *ud) { (void)ud; F.loaded = 0; F.playing = 0; F.closes++; }
static uint64_t f_consumed(void *ud) { (void)ud; return F.pos; }
static const lj_media_host FH = { NULL, f_load, f_play, f_seek, f_close, f_consumed, NULL, NULL };

/* ---- the cue list host: the runner's MP lines go to lj_media_cmd on M (what app/lj_app_io.c does) ---- */
static lj_media M;
static struct { uint32_t cue[16]; int n; } C;
static int h_cue(void *ud, uint32_t n) { (void)ud; if (C.n < 16) C.cue[C.n++] = n; return 0; }
static int h_tc(void *ud, int src, uint32_t *ms) { (void)ud; if (src != LJ_TC_MP || M.state == LJ_MEDIA_CLOSED) return -1; *ms = lj_media_time_cs(&M) * 10u; return 0; }
static int h_media(void *ud, const lj_cl_line *l, uint64_t now) { char e[96]; (void)ud; (void)now; return lj_media_cmd(&M, &FH, l->cmd, l->param, l->param_ms, e, sizeof e); }
static const lj_cl_host H = { NULL, h_cue, NULL, NULL, NULL, NULL, NULL, NULL, NULL, h_tc, NULL, h_media };

static lj_cl_line ln(int cmd, uint32_t ms, const char *pstr, uint32_t cue)
{
    lj_cl_line l;
    memset(&l, 0, sizeof l);
    l.cmd = (uint8_t)cmd; l.param_ms = ms; l.cue = cue;
    if (pstr) lj_str_copy(l.param, sizeof l.param, pstr);
    l.load_error = (uint8_t)lj_cl_cmd_inert(cmd);
    return l;
}
static int has_cue(uint32_t n) { int i; for (i = 0; i < C.n; i++) if (C.cue[i] == n) return 1; return 0; }
static void step(lj_cl_run *r, uint64_t *now) { *now += 10000u; lj_media_step(&M, &FH); lj_cl_run_step(r, *now, &H); }
static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}

int lj_test_media(int argc, char **argv);
int lj_test_media(int argc, char **argv)
{
    char tc[16], err[160];
    lj_cuelist cl, in;
    lj_cl_line rows[8];
    lj_cl_run r;
    uint64_t now = 0;
    FILE *f;
    (void)argc; (void)argv;

    /* W5-MED-1: the clock text */
    CHECK(lj_media_format_tc(tc, sizeof tc, 0) == 0 && strcmp(tc, "00:00:00:00") == 0);
    CHECK(lj_media_format_tc(tc, sizeof tc, 200) == 0 && strcmp(tc, "00:00:02:00") == 0);
    CHECK(lj_media_format_tc(tc, sizeof tc, 3 * 360000u + 25 * 6000u + 7 * 100u + 99u) == 0 && strcmp(tc, "03:25:07:99") == 0);
    CHECK(lj_media_format_tc(tc, 11, 0) == -1 && lj_media_format_tc(tc, sizeof tc, 100u * 360000u) == -1);

    /* W5-MED-2: WAV only, the size cap before the load (hostile names and sizes) */
    CHECK(lj_media_check_file("show.wav", 1000, err, sizeof err) == 0 && lj_media_check_file("C:\\a.b\\SHOW.WAV", LJ_MEDIA_FILE_MAX, err, sizeof err) == 0);
    CHECK(lj_media_check_file("song.mp3", 10, err, sizeof err) == -1 && strcmp(err, LJ_MEDIA_ERR_FORMAT) == 0);
    CHECK(lj_media_check_file("clip.avi", 10, err, sizeof err) == -1 && strcmp(err, LJ_MEDIA_ERR_FORMAT) == 0);
    CHECK(lj_media_check_file("wav", 10, err, sizeof err) == -1 && lj_media_check_file(".wav.mp3", 10, err, sizeof err) == -1 && lj_media_check_file("dir.wav/x", 1, err, sizeof err) == -1);
    CHECK(lj_media_check_file("big.wav", (uint64_t)LJ_MEDIA_FILE_MAX + 1u, err, sizeof err) == -1 && strcmp(err, LJ_MEDIA_ERR_SIZE) == 0);
    CHECK(lj_media_check_file("", 0, err, sizeof err) == -1 && lj_media_check_file(NULL, 0, err, sizeof err) == -1);
    { char longp[400]; memset(longp, 'a', sizeof longp); memcpy(longp + 390, ".wav", 5); CHECK(lj_media_check_file(longp, 1, err, sizeof err) == -1); }

    /* the state machine */
    lj_media_init(&M); memset(&F, 0, sizeof F);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PLAY, NULL, 0, err, sizeof err) == -1 && strcmp(err, LJ_MEDIA_ERR_NOCLIP) == 0);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_LOAD_FILE, "song.mp3", 0, err, sizeof err) == -1 && strcmp(err, LJ_MEDIA_ERR_FORMAT) == 0 && F.loads == 0);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_LOAD_FILE, "missing.wav", 0, err, sizeof err) == -1 && M.state == LJ_MEDIA_CLOSED);
    CHECK(lj_media_cmd(&M, NULL, LJ_CL_MP_LOAD_FILE, "show.wav", 0, err, sizeof err) == -1);   /* headless: no output */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_LOAD_FILE, "show.wav", 0, err, sizeof err) == 0 && M.state == LJ_MEDIA_LOADED && M.rate == F_RATE && M.frames == F_FRAMES);
    CHECK(strcmp(M.recent[0], "show.wav") == 0 && M.host == &FH);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PLAY, NULL, 0, err, sizeof err) == 0 && M.state == LJ_MEDIA_PLAYING && F.playing);
    F.pos = 66150; lj_media_step(&M, &FH); CHECK(lj_media_time_cs(&M) == 150);                     /* 1.50 s of samples */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PAUSE, NULL, 0, err, sizeof err) == 0 && M.state == LJ_MEDIA_PAUSED && !F.playing);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_SEEK, NULL, 4000, err, sizeof err) == 0 && F.pos == 4u * F_RATE && M.cs == 400 && M.state == LJ_MEDIA_PAUSED);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_SEEK, NULL, 99999999u, err, sizeof err) == 0 && F.pos == F_FRAMES);   /* clamped to the end */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_STOP, NULL, 0, err, sizeof err) == 0 && M.state == LJ_MEDIA_STOPPED && F.pos == 0 && M.cs == 0);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PLAY, NULL, 0, err, sizeof err) == 0);
    F.pos = F_FRAMES + 500; lj_media_step(&M, &FH); CHECK(M.state == LJ_MEDIA_ENDED && M.cs == 1000 && !F.playing);   /* never past the clip */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PLAY, NULL, 0, err, sizeof err) == 0 && F.pos == 0 && M.state == LJ_MEDIA_PLAYING);   /* ENDED restarts */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_WAIT_MTC, NULL, 0, err, sizeof err) == -1);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_CLOSE_CLIP, NULL, 0, err, sizeof err) == 0 && M.state == LJ_MEDIA_CLOSED && F.closes >= 1 && M.recent[0][0]);
    lj_media_recent_push(&M, "a.wav"); lj_media_recent_push(&M, "b.wav"); lj_media_recent_push(&M, "show.wav");
    CHECK(!strcmp(M.recent[0], "show.wav") && !strcmp(M.recent[1], "b.wav") && !strcmp(M.recent[2], "a.wav") && M.recent[3][0] == 0);

    /* W5-MED-3 / -4: the runner. 1 Load, 2 Play, 3 MP Wait 00:00:02:00 + cue 5, 4 MP Wait 00:00:05:00 + cue 6, 5 Close Clip */
    lj_media_init(&M); memset(&F, 0, sizeof F); memset(&C, 0, sizeof C);
    rows[0] = ln(LJ_CL_MP_LOAD_FILE, 0, "show.wav", 0);
    rows[1] = ln(LJ_CL_MP_PLAY, 0, NULL, 0);
    rows[2] = ln(LJ_CL_MP_WAIT, lj_cl_hmsf_ms(0, 0, 2, 0, 100), NULL, 5);
    rows[3] = ln(LJ_CL_MP_WAIT, lj_cl_hmsf_ms(0, 0, 5, 0, 100), NULL, 6);
    rows[4] = ln(LJ_CL_MP_CLOSE_CLIP, 0, NULL, 7);
    memset(&cl, 0, sizeof cl); cl.lines = rows; cl.n = cl.cap = 5;
    { uint32_t i; for (i = 0; i < 5; i++) rows[i].line = i + 1; }
    CHECK(lj_cuelist_validate(&cl) == NULL);
    lj_cl_run_init(&r);
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_TC_SRC, LJ_TC_MP, now, &H) == 0 && r.st.tc_src == LJ_TC_MP);
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_TOP, 0, now, &H) == 0);
    CHECK(F.loads == 1 && F.playing && M.state == LJ_MEDIA_PLAYING && r.st.next == 3 && r.st.state == LJ_CL_RUNNING);
    F.pos = 88199; step(&r, &now); CHECK(!has_cue(5) && r.st.next == 3 && r.st.tc_ms == 1990);    /* one frame short of 2.00 s */
    F.pos = 88200; step(&r, &now); CHECK(has_cue(5) && r.st.next == 4);                            /* the sample-exact point */
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_LOG, 0, now, &H) == 0 && r.st.log_ms[0] == 2000);           /* Log time code on MP */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PAUSE, NULL, 0, err, sizeof err) == 0);
    step(&r, &now); step(&r, &now); CHECK(!has_cue(6) && r.st.tc_ms == 2000);                     /* paused: the clock holds */
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_SEEK, NULL, 4990, err, sizeof err) == 0);
    step(&r, &now); CHECK(!has_cue(6) && r.st.tc_ms == 4990);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_SEEK, NULL, 5000, err, sizeof err) == 0);
    step(&r, &now); CHECK(has_cue(6) && has_cue(7) && M.state == LJ_MEDIA_CLOSED && r.st.state == LJ_CL_IDLE);   /* seek moved the wait; Close Clip */
    step(&r, &now); CHECK(r.st.tc_ms == -1);                                                       /* no clip: no MP time code */

    /* Stop Audio/Media Playback when Cuelist stops; a manual MP Wait waits for Go (ignored in Manual Mode) */
    lj_media_init(&M); memset(&F, 0, sizeof F); memset(&C, 0, sizeof C); lj_cl_run_init(&r);
    rows[3].manual = LJ_CL_MANUAL;
    r.stop_media = 1;
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_TOP, 0, now, &H) == 0 && F.playing);
    F.pos = 3 * F_RATE; step(&r, &now); CHECK(has_cue(5) && r.st.state == LJ_CL_WAIT_GO && r.st.next == 4);
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_STOP, 0, now, &H) == 0 && !F.playing && F.pos == 0 && M.state == LJ_MEDIA_STOPPED);
    r.stop_media = 0; F.playing = 1; M.state = LJ_MEDIA_PLAYING;
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_TOP, 0, now, &H) == 0 && lj_cl_run_op(&r, &cl, LJ_CL_OP_STOP, 0, now, &H) == 0 && F.playing);   /* preference off: plays on */
    rows[3].manual = LJ_CL_AUTO;

    /* Auto seek audio tracks/media file on Run: Run from line 4 seeks to the last MP Wait of lines 1..4 (5.00 s) */
    lj_media_init(&M); memset(&F, 0, sizeof F); lj_cl_run_init(&r);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_LOAD_FILE, "show.wav", 0, err, sizeof err) == 0);
    r.autoseek_media = 1;
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_RUN, 4, now, &H) == 0 && F.pos == 5u * F_RATE && F.seeks == 1);
    lj_cl_run_init(&r);
    CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_RUN, 2, now, &H) == 0 && F.seeks == 1);                 /* no MP Wait above line 2: no seek */

    /* no media host: MP lines stay inert (the replay harness) */
    { lj_cl_host h0 = H; h0.media = NULL; memset(&C, 0, sizeof C); lj_cl_run_init(&r);
      CHECK(lj_cl_run_op(&r, &cl, LJ_CL_OP_TOP, 0, now, &h0) == 0 && C.n == 0 && r.st.state == LJ_CL_IDLE); }   /* every line skipped, no cue latched */

    /* .Cl2: MP lines load clean and round-trip byte-unchanged; a missing clip and a CD line load red (ME-11) */
    f = fopen("lj_test_media_clip.wav", "wb"); CHECK(f != NULL); if (f) { fputs("RIFF", f); fclose(f); }
    rows[0] = ln(LJ_CL_MP_LOAD_FILE, 0, "lj_test_media_clip.wav", 1);
    rows[4] = ln(LJ_CL_CD_STOP, 0, NULL, 0);
    { uint32_t i; for (i = 0; i < 5; i++) rows[i].line = i + 1; }
    lj_str_copy(cl.name, sizeof cl.name, "Media");
    CHECK(lj_cuelist_save("lj_test_media_a.Cl2", &cl) == 0);
    memset(&in, 0, sizeof in);
    CHECK(lj_cuelist_load("lj_test_media_a.Cl2", &in, err, sizeof err) == 0 && in.n == 5);
    if (in.n == 5) {
        CHECK(!in.lines[0].load_error && !in.lines[1].load_error && !in.lines[2].load_error && in.lines[4].load_error);   /* MP live, CD red */
        CHECK(in.lines[2].cmd == LJ_CL_MP_WAIT && in.lines[2].param_ms == 2000 && !strcmp(in.lines[0].param, "lj_test_media_clip.wav"));
        CHECK(lj_cuelist_save("lj_test_media_b.Cl2", &in) == 0 && same_file("lj_test_media_a.Cl2", "lj_test_media_b.Cl2") == 1);
        CHECK(lj_cl_line_state(NULL, &in, 1) == LJ_CLL_PLAIN && lj_cl_line_state(NULL, &in, 5) == LJ_CLL_ERROR);
    }
    free(in.lines);
    remove("lj_test_media_clip.wav");
    memset(&in, 0, sizeof in);
    CHECK(lj_cuelist_load("lj_test_media_a.Cl2", &in, err, sizeof err) == 0 && in.n == 5 && in.lines[0].load_error == 1);   /* the clip is gone: red, kept */
    if (in.n == 5) CHECK(lj_cuelist_save("lj_test_media_b.Cl2", &in) == 0 && same_file("lj_test_media_a.Cl2", "lj_test_media_b.Cl2") == 1);   /* written back unchanged */
    free(in.lines);
    remove("lj_test_media_a.Cl2"); remove("lj_test_media_b.Cl2");
    CHECK(lj_cl_cmd_inert(LJ_CL_MP_PLAY) == 0 && lj_cl_cmd_inert(LJ_CL_CD_STOP) == 1 && lj_cl_cmd_inert(LJ_CL_DA_PLAY) == 1);

    printf("test_media: %s (%d failures)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}

/* ---------------------------------------------------------------- the windows */
static void golden(lj_window *win, const char *name)
{
    char g[1024], tmp[1024];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    if (snprintf(g, sizeof g, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name) < 0) g[0] = 0;
    if (snprintf(tmp, sizeof tmp, "lj_test_media_%s.bmp", name) < 0) tmp[0] = 0;
    lj_window_paint(win);
    if (upd && upd[0] == '1') { CHECK(lj_bmp_write(lj_window_surface(win), g) == 0); printf("  %s: written\n", g); return; }
    CHECK(lj_bmp_write(lj_window_surface(win), tmp) == 0);
    rc = same_file(tmp, g);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (the render is %s)\n", name, rc ? "missing or unreadable" : "differs", tmp); return; }
    printf("  %s: identical\n", name);
    remove(tmp);
}
static int item_enabled(lj_window *w, const char *n) { lj_menu_rt *rt = lj_menu_rt_named(lj_window_menus(w), n); return rt ? (int)rt->enabled : -1; }
static int item_visible(lj_window *w, const char *n) { lj_menu_rt *rt = lj_menu_rt_named(lj_window_menus(w), n); return rt ? (int)rt->visible : -1; }
static lj_cl_status ST;
static int nop_ops[32], n_ops;
static void on_op(enum lj_desktop_action a, int arg, void *ud) { (void)a; (void)ud; if (n_ops < 32) nop_ops[n_ops++] = arg; }

int lj_test_media_form(int argc, char **argv);
int lj_test_media_form(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_window *win, *cw;
    lj_widget *w;
    lj_cuelist *cl;
    lj_cl_line l;
    char row[200], err[96];
    (void)argc; (void)argv;
    ctx->show = lj_show_new();
    ctx->forms_ini = lj_ini_new();
    ctx->main_ini = lj_ini_new();
    lj_media_init(&M); memset(&F, 0, sizeof F);
    M.host = &FH;
    ctx->media = &M;
    memset(&ST, 0, sizeof ST); ST.tc_src = LJ_TC_PC_ELAPSED; ST.tc_ms = -1;
    ctx->cl = &ST;
    lj_desktop_bind_action(LJ_ACT_CUELIST_OP, on_op, NULL);

    /* W5-MED-5: the player window */
    win = lj_media_form_open(LJ_WIN_MEDIA_PLAYER);
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(lj_media_form_get(LJ_WIN_MEDIA_PLAYER) == win && strcmp(lj_window_caption(win)->text, "LightJockey MediaPlayer") == 0);
    CHECK((w = lj_window_find(win, "sbPlay")) != NULL && !w->enabled && !lj_window_find(win, "sbStop")->enabled && !lj_window_find(win, "sbScanFw")->enabled);
    CHECK(item_enabled(win, "MM_Playback") == 0 && item_enabled(win, "ScreenSize1") == 0 && item_enabled(win, "ClipSize1") == 0 && item_enabled(win, "M_RecentPlayBackFiles") == 0);
    CHECK(item_enabled(win, "LoadMediaClip1") == 1 && item_enabled(win, "MinimumSize1") == 1 && item_enabled(win, "M_Stayontop") == 1);
    CHECK(item_visible(win, "M_Recent0") == 0);
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_LOAD_FILE, "show.wav", 0, err, sizeof err) == 0);
    F.pos = 3u * F_RATE + 22050u; lj_media_step(&M, &FH);
    lj_media_form_refresh(LJ_WIN_MEDIA_PLAYER);
    CHECK(lj_window_find(win, "sbPlay")->enabled && !lj_window_find(win, "sbPause")->enabled && lj_window_find(win, "sbStop")->enabled);
    CHECK(strcmp(((lj_w_cap *)lj_window_find(win, "pTimeCode"))->caption, "00:00:03:50") == 0);
    CHECK(strcmp(lj_w_edit_text(lj_window_find(win, "eMediaFile")), "show.wav") == 0);
    CHECK(item_visible(win, "M_Recent0") == 1 && item_visible(win, "M_Recent1") == 0 && strcmp(lj_ini_gets(ctx->forms_ini, "LJMEDIAPLAYER", "File0", ""), "show.wav") == 0);
    golden(win, "W5MED_MEDIA_PLAYER");
    CHECK(lj_media_cmd(&M, &FH, LJ_CL_MP_PLAY, NULL, 0, err, sizeof err) == 0);
    lj_media_form_refresh(LJ_WIN_MEDIA_PLAYER);
    CHECK(!lj_window_find(win, "sbPlay")->enabled && lj_window_find(win, "sbPause")->enabled);
    lj_media_form_close(LJ_WIN_MEDIA_PLAYER);
    CHECK(lj_media_form_get(LJ_WIN_MEDIA_PLAYER) == NULL);

    /* W5-MED-3: the Cuelist window's MP rows live, CD / DA greyed with the hint (ME-11) */
    cl = lj_cuelist_form_list();
    CHECK(cl != NULL);
    if (!cl) return 1;
    memset(&l, 0, sizeof l); lj_cuelist_insert(cl, cl->n, &l); lj_cuelist_insert(cl, cl->n, &l);
    cw = lj_cuelist_form_open();
    CHECK(cw != NULL);
    if (!cw) return 1;
    CHECK(item_enabled(cw, "Pop_MPCommands") == 1 && item_enabled(cw, "CueMediaFile1") == 1 && item_enabled(cw, "WaitMediaPlayer1") == 1 && item_enabled(cw, "MediaPlayer1") == 1);
    CHECK(item_enabled(cw, "OffsetMediaplayerTimecodes1") == 1 && item_enabled(cw, "SortMediaCodes1") == 1);
    CHECK(item_enabled(cw, "Pop_CDCommands") == 0 && item_enabled(cw, "pop_DACommands") == 0 && item_enabled(cw, "OffsetCDTimecodes1") == 0 && item_enabled(cw, "SortDAWaitTimes") == 0);
    CHECK((w = lj_window_find(cw, "sbMPTime")) != NULL && w->enabled && w->hint && strcmp(w->hint, "Show Media Timecode") == 0);
    CHECK((w = lj_window_find(cw, "sbCDTime")) != NULL && !w->enabled && w->hint && strcmp(w->hint, LJ_CL_NOT_AVAILABLE) == 0);
    CHECK((w = lj_window_find(cw, "sbDATime")) != NULL && !w->enabled);
    lj_w_listview_select(lj_window_find(cw, "lv1"), 0); lj_cuelist_form_refresh();
    lj_cuelist_form_apply_cmd(LJ_CL_MP_WAIT);
    CHECK(cl->lines[0].cmd == LJ_CL_MP_WAIT && strcmp(lj_cuelist_form_edit_panel(), "pWaitMPTime") == 0);
    lj_w_edit_set_text(lj_window_find(cw, "eWaitMPSec"), "2");
    lj_w_edit_set_text(lj_window_find(cw, "eWaitMPHour"), "45");                                  /* past UpDown25 Max 23: clamped */
    CHECK(cl->lines[0].param_ms == lj_cl_hmsf_ms(23, 0, 2, 0, 100));
    CHECK(lj_cuelist_form_row(1, row, sizeof row) == 0 && strcmp(row, "1\tAuto\tWait for MP at\t23:00:02:00\t\t") == 0);
    lj_cuelist_form_apply_cmd(LJ_CL_CD_STOP); CHECK(cl->lines[0].cmd == LJ_CL_MP_WAIT);            /* CD: refused (ME-11) */
    lj_w_listview_select(lj_window_find(cw, "lv1"), 1); lj_cuelist_form_refresh();
    lj_cuelist_form_apply_cmd(LJ_CL_MP_LOAD_FILE);
    CHECK(cl->lines[1].cmd == LJ_CL_MP_LOAD_FILE && strcmp(lj_cuelist_form_edit_panel(), "pCueMP") == 0);
    CHECK(lj_cuelist_form_set_mp_file(2, "not-there.wav") == 0 && cl->lines[1].load_error == 1 && strcmp(cl->lines[1].param, "not-there.wav") == 0);
    CHECK(lj_cuelist_form_set_mp_file(1, "x.wav") == -1);                                          /* not an MP Load File line */
    CHECK(lj_cuelist_validate(cl) == NULL);
    n_ops = 0;
    lj_show_dlg_timecode_offset(LJ_CLF_MP);
    CHECK(lj_win_get(LJ_WIN_TIMECODE_OFFSET) != NULL);
    if (lj_win_get(LJ_WIN_TIMECODE_OFFSET)) {
        CHECK(lj_window_find(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "pMP")->visible && !lj_window_find(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "pMidi")->visible);
        CHECK(item_enabled(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "M_MP") == 1 && item_enabled(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "M_CD") == 0);
    }
    { int over, nl = 0; cl->lines[0].param_ms = 1000; over = lj_show_dlg_offset(cl, LJ_CLF_MP, 1500, 1, &nl); CHECK(over == 0 && nl == 1 && cl->lines[0].param_ms == 2500); }
    lj_cuelist_form_close();

    printf("test_media_form: %s (%d failures)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}
