/* test_audio_form.c - `lj4evr_tests test_audio_form`: the audio track's windows headless (W5-AUD-1, -3, -4, -5).
 *  - app/lj_app_io.c started with a fake platform (two synthetic devices, an open that can be told to fail);
 *  - the main window shows &Audio; Start Audio Analyzer opens TLJSoundApp with every DFM handler bound (ME-16);
 *  - the Analyzer menus toggle ctx->audio and the panels; the faders set decay / sensitivity / noise / gain; Auto Save
 *    writes Hardware.Ini [AUDIO] on close;
 *  - Hardware Setup: tsAudio lists "Default device" first then the devices, Enabled + a device persist and open the
 *    capture on that device; a failed open shows "Audio-in failed" on the status bar (W5-AUD-5), never a crash;
 *  - audio_inject (the UI script verb) trigs the analyzer: the status-bar Trig panel lights and an audio slot advances;
 *  - golden W5AU_ANALYZER (Time-line view with synthetic levels and trigs), LJ_GOLDEN_UPDATE=1 rewrites it. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_app_io.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_main_window.h"
#include "desktop/lj_statusbar.h"
#include "engine/lj_engine.h"
#include "gfx/lj_bmp.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_audio_form.h"
#include "windows/lj_hardware_setup.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_audio_form_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char bad_log[512];
static void sink(void *ud, const char *line)
{
    (void)ud;
    if ((strstr(line, "no binding for handler") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler")) && !bad_log[0]) snprintf(bad_log, sizeof bad_log, "%s", line);
}

/* ---- the fake platform ---- */
static struct { int opens, closes, fail; char dev[LJ_AUDIO_DEV_NAME]; } fake;
static int fake_list(lj_audio_dev *out, int max)
{
    static const char *const n[] = { "Microphone (Synthetic)", "Line In (Synthetic)" };
    int i;
    for (i = 0; i < 2 && i < max; i++) lj_str_copy(out[i].name, sizeof out[i].name, n[i]);
    return i;
}
static int fake_open(const char *device)
{
    fake.opens++; lj_str_copy(fake.dev, sizeof fake.dev, device ? device : "");
    if (fake.fail) { lj_audio_set_device_status(0, "Audio-in failed: permission denied (synthetic)"); return -1; }
    lj_audio_set_device_status(1, "");
    return 0;
}
static void fake_close(void) { fake.closes++; lj_audio_set_device_status(0, ""); }

static void menu_fire(lj_window *win, const char *name)
{
    lj_menu *m = win ? lj_window_menus(win) : NULL;
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, name) : NULL;
    CHECK(rt && rt->fn);
    if (!rt || !rt->fn) return;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
}
static int menu_checked(lj_window *win, const char *name) { lj_menu_rt *rt = win ? lj_menu_rt_named(lj_window_menus(win), name) : NULL; return rt ? rt->checked : -1; }
static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(lj_window *w, const char *name)
{
    char path[1024], tmp[256];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    if (!w) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    snprintf(path, sizeof path, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, TMP "%s.bmp", name);
    lj_window_invalidate_all(w);
    lj_window_paint(w);
    if (lj_bmp_write(lj_window_surface(w), upd && upd[0] == '1' ? path : tmp) != 0) { fails++; printf("FAIL golden %s: write\n", name); return; }
    if (upd && upd[0] == '1') { printf("  golden %s written\n", name); return; }
    rc = same_file(tmp, path);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (new render at %s)\n", name, rc == 0 ? "differs" : "missing", tmp); }
}
static int run_script(const char *text)
{
    FILE *f = fopen(TMP "audio.lj4s", "wb");
    uint64_t t0 = lj_clock_us();
    int r;
    if (!f) return -2;
    fputs(text, f); fclose(f);
    if ((r = lj_uiscript_load(TMP "audio.lj4s")) != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 2000000u) {}
    return r;
}

static void test_menu_and_window(lj_window *desk)
{
    lj_menu *m = desk ? lj_window_menus(desk) : NULL;
    lj_menu_rt *rt;
    lj_window *w;
    lj_ctx *c = lj_ctx_get();
    CHECK(m && (rt = lj_menu_rt_named(m, "M_Audio")) != NULL && rt->visible);          /* &Audio shown (fidelity row 10) */
    CHECK(m && (rt = lj_menu_rt_named(m, "M_RestartSound")) != NULL && rt->enabled);
    CHECK(m && (rt = lj_menu_rt_named(m, "M_StartDAPlayer")) != NULL && !rt->enabled); /* Digital Audio stays dropped (I5) */
    bad_log[0] = 0;
    menu_fire(desk, "M_RestartSound");                                                 /* Start Audio Analyzer */
    w = lj_win_get(LJ_WIN_AUDIO_ANALYZER);
    CHECK(w != NULL && strcmp(lj_window_desc(w)->class_name, "TLJSoundApp") == 0);
    CHECK(strcmp(lj_window_caption(w)->text, "Martin Audio Analyzer") == 0);
    CHECK(bad_log[0] == 0);
    if (bad_log[0]) printf("  log: %s\n", bad_log);
    if (!w) return;
    CHECK(lj_window_find(w, "ComboBox1") && ((lj_w_combobox *)lj_window_find(w, "ComboBox1"))->n == 3);
    CHECK(strcmp(((lj_w_combobox *)lj_window_find(w, "ComboBox1"))->items[0], "Default device") == 0);
    CHECK(!lj_window_find(w, "FFTPanel")->visible && lj_window_find(w, "FaderPanel")->visible);
    menu_fire(w, "UseFFT1");                                                             /* Trigger > Use Equalizer: the spectrum panel appears */
    CHECK(c->audio->use_eq == 1 && lj_window_find(w, "FFTPanel")->visible && menu_checked(w, "UseFFT1") == 1);
    menu_fire(w, "High1");
    CHECK(c->audio->noise_range == LJ_AUDIO_NOISE_HIGH && menu_checked(w, "High1") == 1 && menu_checked(w, "Medium1") == 0);
    menu_fire(w, "Timeline1");
    CHECK(c->audio->view == LJ_AUDIO_VIEW_TIMELINE && menu_checked(w, "Timeline1") == 1);
    menu_fire(w, "Triggerfaders1");
    CHECK(!c->audio->trigger_faders && !lj_window_find(w, "FaderPanel")->visible);
    menu_fire(w, "Triggerfaders1");
    lj_w_trackbar_set(lj_window_find(w, "TrackBar1"), 25); lj_w_emit(lj_window_find(w, "TrackBar1"), LJ_WEV_CHANGE);
    CHECK(c->audio->decay_ms == 600);
    lj_w_trackbar_set(lj_window_find(w, "TrackBar2"), 300); lj_w_emit(lj_window_find(w, "TrackBar2"), LJ_WEV_CHANGE);
    CHECK(c->audio->sensitivity_x10 == 20);                                             /* the most aggressive end = factor 2.0 */
    lj_w_trackbar_set(lj_window_find(w, "TrackBar3"), 455); lj_w_emit(lj_window_find(w, "TrackBar3"), LJ_WEV_CHANGE);
    CHECK(c->audio->noise == 255);
    lj_w_scrollbar_set(lj_window_find(w, "ScrollBar1"), 127); lj_w_emit(lj_window_find(w, "ScrollBar1"), LJ_WEV_CHANGE);
    CHECK(c->audio->gain_pct == 1000);
    CHECK(strncmp(lj_window_find(w, "ScrollBar1")->hint, "Gain: 20", 8) == 0 && strstr(lj_window_find(w, "ScrollBar1")->hint, "0dB|Global volume setting"));
    menu_fire(w, "Volume1");                                                            /* Set Default > Gain */
    CHECK(c->audio->gain_pct == 100 && ((lj_w_scrollbar *)lj_window_find(w, "ScrollBar1"))->sb.pos == 0);
    c->audio->eq[4] = 3; menu_fire(w, "Equalizer1"); CHECK(c->audio->eq[4] == 128);
    menu_fire(w, "AutoSaveChanges1");
    CHECK(c->audio->auto_save == 1);
    lj_win_close(LJ_WIN_AUDIO_ANALYZER);                                                /* Auto Save: [AUDIO] written on exit */
    CHECK(lj_ini_geti(c->hardware_ini, "AUDIO", "UseFFT", -1) == 1 && lj_ini_geti(c->hardware_ini, "AUDIO", "NoiseLevelSwitch", -1) == 2);
    CHECK(lj_ini_geti(c->hardware_ini, "AUDIO", "TrigDecay", -1) == 600 && lj_ini_geti(c->hardware_ini, "AUDIO", "AutoSaveChanges", -1) == 1);
}

static void test_hardware_page(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_window *hs = lj_win_open(LJ_WIN_HARDWARE_SETUP);
    lj_widget *cb = hs ? lj_window_find(hs, "cbAudioDevice") : NULL, *rg = hs ? lj_window_find(hs, "RGAudio") : NULL;
    CHECK(hs && cb && rg);
    if (!cb || !rg) return;
    CHECK(((lj_w_combobox *)cb)->n == 3 && strcmp(((lj_w_combobox *)cb)->items[1], "Microphone (Synthetic)") == 0 && ((lj_w_combobox *)cb)->index == 0);
    CHECK(rg->enabled && lj_w_radiogroup_index(rg) == 0);                              /* W5-X-OFF: Disabled on a fresh Hardware.Ini */
    CHECK(!lj_window_find(hs, "rgAudioCD")->enabled && !lj_window_find(hs, "eWinamp")->enabled);   /* CD-Audio / Winamp stay dropped */
    lj_w_radiogroup_set_index(rg, 1);
    ((lj_w_combobox *)cb)->index = 2;
    fake.opens = 0;
    lj_w_click(lj_window_find(hs, "bOk"));
    CHECK(lj_win_get(LJ_WIN_HARDWARE_SETUP) == NULL);
    CHECK(c->audio->enabled && strcmp(c->audio->device, "Line In (Synthetic)") == 0);
    CHECK(fake.opens == 1 && strcmp(fake.dev, "Line In (Synthetic)") == 0);
    CHECK(lj_ini_geti(c->hardware_ini, "AUDIO", "Enabled", 0) == 1 && strcmp(lj_ini_gets(c->hardware_ini, "AUDIO", "Device", ""), "Line In (Synthetic)") == 0);
    {   /* the persisted file reloads the same way (enable persists) */
        lj_ini *back = lj_ini_load(TMP "Hardware.Ini");
        lj_audio_cfg r;
        lj_audio_cfg_load(back, &r);
        CHECK(back && r.enabled && strcmp(r.device, "Line In (Synthetic)") == 0);
        lj_ini_free(back);
    }
    lj_statusbar_refresh();
    CHECK(strcmp(lj_statusbar_panel_text(LJ_SB_STATUS), "Running..") == 0);
    fake.fail = 1;                                                                       /* W5-AUD-5: a denied microphone */
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_AUDIO);
    lj_statusbar_refresh();
    CHECK(strcmp(lj_statusbar_panel_text(LJ_SB_STATUS), "Audio-in failed") == 0);
    lj_win_open(LJ_WIN_AUDIO_ANALYZER);
    CHECK(strstr(((lj_w_statusbar *)lj_window_find(lj_win_get(LJ_WIN_AUDIO_ANALYZER), "StatusBar1"))->text[2], "permission denied") != NULL);
    lj_win_close(LJ_WIN_AUDIO_ANALYZER);
    fake.fail = 0;
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_AUDIO);
    lj_statusbar_refresh();
    CHECK(strcmp(lj_statusbar_panel_text(LJ_SB_STATUS), "Running..") == 0);
}

static void test_trigs(lj_engine *e)
{
    static lj_sequence q; static lj_scene sc[3]; static lj_cue cue;
    lj_ctx *c = lj_ctx_get();
    const lj_runner *r;
    lj_cmd cmd;
    uint64_t now = 5000000;
    int i;
    memset(&q, 0, sizeof q); memset(sc, 0, sizeof sc);
    q.number = 1; q.n = 3; q.scenes = sc;
    for (i = 0; i < 3; i++) { sc[i].scene_ms = 60000; sc[i].val[0] = (uint8_t)(10 * (i + 1)); sc[i].state[0] = LJ_SNAP; }
    memset(&cue, 0, sizeof cue); cue.number = 1; cue.slot[0].seq = 1; cue.slot[0].enabled = 1; cue.slot[0].trig = LJ_TRIG_AUDIO; cue.slot[0].audio_trigs_per_trig = 1;
    memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 1; cmd.cue = &cue; cmd.seq[0] = &q;
    lj_ctx_post(&cmd);
    lj_lock(); lj_engine_tick(e, now += 25000); lj_unlock();
    r = lj_engine_runner(e, LJ_RUN_SLOT0);
    CHECK(r->scene == 0);
    lj_audio_cfg_default(c->audio); c->audio->enabled = 1;
    lj_audio_reset(c->audio_state);
    CHECK(run_script("audio_inject 6000 4\nquit\n") == 0);                              /* one onset, then the decay holds */
    CHECK(c->audio_state->trigs == 1);
    lj_lock(); lj_engine_tick(e, now += 25000); lj_unlock();
    CHECK(r->scene == 1);
    lj_statusbar_refresh();
    CHECK(lj_audio_form_trig_lit(lj_clock_us()) == 1);
    CHECK(run_script("audio_inject 100 2\nquit\n") == 0 && c->audio_state->trigs == 1);   /* under the threshold: no trig */
}

static void test_golden(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_window *w;
    int i;
    lj_audio_cfg_default(c->audio);
    c->audio->enabled = 1; c->audio->view = LJ_AUDIO_VIEW_TIMELINE; c->audio->use_eq = 1;
    lj_audio_reset(c->audio_state);
    for (i = 0; i < LJ_AUDIO_TIMELINE; i++) {                                          /* a synthetic beat: a peak every 43 blocks (120 BPM) over a 400 floor */
        int k = i % 43;
        lj_audio_detect(c->audio_state, c->audio, (uint16_t)(k == 0 ? 5000 : k < 4 ? 2600 - k * 500 : 400 + (i * 37) % 300), 1000000u + (uint64_t)i * 11610u);
    }
    for (i = 0; i < LJ_AUDIO_BANDS; i++) { c->audio_state->band[i] = (uint16_t)(6000 - i * 340); c->audio->eq[i] = (uint8_t)(i < 3 ? 200 : 128); }
    c->audio_state->clip = 1;
    w = lj_win_open(LJ_WIN_AUDIO_ANALYZER);
    golden(w, "W5AU_ANALYZER");
    lj_win_close(LJ_WIN_AUDIO_ANALYZER);
    c->audio->use_eq = 0; c->audio->view = LJ_AUDIO_VIEW_REALTIME;
    for (i = 0; i < LJ_AUDIO_BLOCK; i++) c->audio_state->wave[i] = (int16_t)((i * 997) % 61000 - 30500);
    c->audio_state->clip = 2;
    w = lj_win_open(LJ_WIN_AUDIO_ANALYZER);
    golden(w, "W5AU_ANALYZER_REALTIME");
    lj_win_close(LJ_WIN_AUDIO_ANALYZER);
}

int lj_test_audio_form(int argc, char **argv);
int lj_test_audio_form(int argc, char **argv)
{
    static lj_sacn_cfg sacn;
    lj_ctx *c = lj_ctx_get();
    lj_engine *e = lj_engine_create();
    lj_app_io_platform plat;
    lj_window *desk;
    (void)argc; (void)argv;
    if (!e) { printf("FAIL lj_engine_create\n"); return 1; }
    lj_log_set_sink(sink, NULL);
    lj_fs_mkdir(TMP);
    remove(TMP "Hardware.Ini");
    snprintf(c->userdir, sizeof c->userdir, TMP);
    c->hardware_ini = lj_ini_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    memset(&sacn, 0, sizeof sacn); c->sacn = &sacn; c->engine = e;
    memset(&plat, 0, sizeof plat); plat.audio_list = fake_list; plat.audio_open = fake_open; plat.audio_close = fake_close;
    lj_win_set_host(NULL);
    lj_app_io_set_platform(&plat);
    lj_app_io_start();
    CHECK(c->audio && !c->audio->enabled && fake.opens == 0);                           /* W5-X-OFF */
    desk = lj_desktop_create();
    CHECK(desk != NULL);

    test_menu_and_window(desk);
    test_hardware_page();
    test_trigs(e);
    test_golden();

    lj_app_io_stop();
    lj_win_close_all();
    if (desk) lj_desktop_destroy(desk);
    lj_ini_free(c->hardware_ini); lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->hardware_ini = c->main_ini = c->forms_ini = NULL;
    c->sacn = NULL; c->engine = NULL;
    lj_engine_destroy(e);
    lj_log_set_sink(NULL, NULL);
    printf("test_audio_form: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
