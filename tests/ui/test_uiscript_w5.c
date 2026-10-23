/* test_uiscript_w5.c - `lj4evr_tests test_uiscript_w5`: the W5 app seam headless (docs/w5-contracts.md seams 4, 10, 12).
 *  - app/lj_app_io.c fills the W5 lj_ctx records, opens nothing on a default Hardware.Ini (W5-X-OFF: no socket bound,
 *    no device), and Emulate Audio Trig (LJ_FN_EMULATE_AUDIO_TRIG) / LJ_ACT_AUDIO_TRIG post the audio trig the engine
 *    counts (one scene per Audio-Trigs / Trig).
 *  - the four W5 UI script verbs pass their load-time checks with good arguments and refuse bad ones; executing them
 *    never crashes whether or not the tracks' modules have landed. */
#include <stdio.h>
#include <string.h>
#include "app/lj_app_io.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_lock.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "io/lj_fn.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define TMP "test_uiscript_w5_tmp/"

static int load_text(const char *text)
{
    FILE *f = fopen(TMP "w5.lj4s", "wb");
    if (!f) return -2;
    fputs(text, f); fclose(f);
    return lj_uiscript_load(TMP "w5.lj4s");
}
static int run_text(const char *text)
{
    uint64_t t0 = lj_clock_us();
    int r;
    if ((r = load_text(text)) != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 2000000u) {}
    return r;
}

int lj_test_uiscript_w5(int argc, char **argv)
{
    static lj_sequence q; static lj_scene sc[3]; static lj_cue cue; static lj_sacn_cfg sacn;
    lj_ctx *c = lj_ctx_get();
    lj_engine *e = lj_engine_create();
    const lj_runner *r;
    uint64_t now = 1000000;
    lj_cmd cmd;
    int i;
    (void)argc; (void)argv;
    if (!e) { printf("FAIL lj_engine_create\n"); return 1; }
    lj_fs_mkdir(TMP);
    memset(&sacn, 0, sizeof sacn);
    c->engine = e; c->sacn = &sacn;
    lj_app_io_set_platform(NULL);
    lj_app_io_start();
    CHECK(c->dmxin && c->dmxin_map && c->audio && c->audio_state && c->remote && c->extapp && c->media);
    CHECK(!c->dmxin->enabled && !c->audio->enabled && !c->remote->tcp_on && !c->remote->serial_on);   /* W5-X-OFF on a fresh Hardware.Ini */
    CHECK(c->remote->tcp_ip == 0x7F000001u);                                                          /* bound to 127.0.0.1 unless the user picks an interface */

    memset(&q, 0, sizeof q); memset(sc, 0, sizeof sc);
    q.number = 1; q.n = 3; q.scenes = sc;
    for (i = 0; i < 3; i++) { sc[i].scene_ms = 60000; sc[i].val[0] = (uint8_t)(10 * (i + 1)); sc[i].state[0] = LJ_SNAP; }
    memset(&cue, 0, sizeof cue); cue.number = 1; cue.slot[0].seq = 1; cue.slot[0].enabled = 1; cue.slot[0].trig = LJ_TRIG_AUDIO; cue.slot[0].audio_trigs_per_trig = 1;
    memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 1; cmd.cue = &cue; cmd.seq[0] = &q;
    lj_ctx_post(&cmd);
    lj_lock(); lj_engine_tick(e, now += 25000); lj_unlock();
    r = lj_engine_runner(e, LJ_RUN_SLOT0);
    CHECK(r->scene == 0);
    lj_fn_run(LJ_FN_EMULATE_AUDIO_TRIG, 1);                                     /* the hotkey row: the same command an analyzer trig posts */
    lj_lock(); lj_engine_tick(e, now += 25000); lj_unlock();
    CHECK(r->scene == 1);
    lj_desktop_run_action(LJ_ACT_AUDIO_TRIG, 0);
    lj_lock(); lj_engine_tick(e, now += 25000); lj_unlock();
    CHECK(r->scene == 2);
    lj_fn_run(LJ_FN_EMULATE_AUDIO_TRIG, 0);                                     /* a release does nothing */
    lj_lock(); lj_engine_tick(e, now += 25000); lj_unlock();
    CHECK(r->scene == 2);

    CHECK(load_text("sacn_in 10 FF8000 01\nartnet_in 258 FF\naudio_inject 12000 4\nremote 129 1 2\n") == 0);
    CHECK(load_text("sacn_in 0 FF\n") != 0);                                   /* universe 0 */
    CHECK(load_text("sacn_in 64000 FF\n") != 0);
    CHECK(load_text("sacn_in 10 F\n") != 0);                                   /* half a byte */
    CHECK(load_text("artnet_in 32768 00\n") != 0);
    CHECK(load_text("audio_inject 40000\n") != 0);
    CHECK(load_text("audio_inject 100 0\n") != 0);
    CHECK(load_text("remote 300\n") != 0);
    CHECK(load_text("remote\n") != 0);
    run_text("audio_inject 12000 4\nquit\n");                                   /* executes whatever the audio track has landed */
    run_text("sacn_in 10 FF8000\nquit\n");                                     /* refused while DMX-in is off or pending; never crashes */
    run_text("remote 129 1 2\nquit\n");

    lj_app_io_stop();
    c->engine = NULL; c->sacn = NULL;
    lj_engine_destroy(e);
    printf("test_uiscript_w5: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
