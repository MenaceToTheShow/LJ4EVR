/* test_chase.c - `lj4evr_tests test_chase`: the global chase timer (LORD_ENGINE-19, 11 F10, E9). Two CHASE_TAP
 * stamps 30 ms apart (at_us, the UI's 1 ms timer) set one period; a chase-mode manual slot then advances on the
 * absolute grid tap + 30 ms * n, each advance seen on the first 25 ms tick at or after its boundary (+-1 tick),
 * with no drift over 2 s; the period rounds to 1 ms, is not scaled by the cue time (F10), carries across a cue
 * load (global rate), and two manual TRIGs set it too. chase fade := period unless DisableChaseFade.
 * `lj4evr_tests test_lock_hold` (ME-81, design 3.1): one tick under lj_lock() with 4 cues loaded in turn, 20
 * statics and a 5-slot BG cue stays under LJ_LOCK_HOLD_TEST_US (a quarter tick); the "lock: held" lines lj_unlock
 * logs in Debug builds are counted and reported. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static lj_profile s_dim;

static void lj_t_cmd(lj_engine *e, int kind, uint32_t a, uint32_t b, uint32_t c, uint64_t at_us)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = c; cmd.at_us = at_us;
    lj_engine_cmd(e, &cmd);
}

static void lj_t_load(lj_engine *e, const lj_cue *cue, const lj_sequence *seq)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = cue->number; cmd.cue = cue; cmd.seq[0] = seq;
    lj_engine_cmd(e, &cmd);
}

static lj_show *lj_t_show(int nfix, int footprint)
{
    lj_show *show = (lj_show *)calloc(1, sizeof *show);
    int i;
    memset(&s_dim, 0, sizeof s_dim);
    s_dim.footprint = (uint8_t)footprint; s_dim.use_gm = 1; s_dim.intensity.enabled = 1; s_dim.intensity.max = 255;
    for (i = 0; i < footprint; i++) s_dim.channel_class[i] = i % 4 ? LJ_CH_COLOR : LJ_CH_INTENSITY;
    if (show) for (i = 0; i < nfix; i++) {
        show->fixture[i].kind = LJ_FX_BUILTIN;
        show->fixture[i].link = (uint8_t)(1 + (i * footprint) / LJ_LINK_CH); show->fixture[i].address = (uint16_t)(1 + (i * footprint) % LJ_LINK_CH);
    }
    return show;
}

int lj_test_chase(int argc, char **argv);
int lj_test_chase(int argc, char **argv)
{
    enum { T_TICK = LJ_TICK_US_DEFAULT, T_SCENES = 4 };
    lj_show *show = lj_t_show(1, 1);
    lj_engine *e = lj_engine_create();
    lj_sequence seq; lj_scene sc[T_SCENES]; lj_cue cue, cue2;
    lj_engine_status st;
    const lj_runner *r;
    uint64_t now = 0, t_tap = 130000, due;
    int i, k, advances = 0, last_scene;
    (void)argc; (void)argv;
    if (!show || !e) { printf("FAIL alloc\n"); free(show); lj_engine_destroy(e); return 1; }
    memset(&seq, 0, sizeof seq); memset(sc, 0, sizeof sc); memset(&cue, 0, sizeof cue);
    seq.number = 1; seq.n = T_SCENES; seq.scenes = sc;
    for (i = 0; i < T_SCENES; i++) { sc[i].scene_ms = 60000; sc[i].fade_ms = 2000; sc[i].val[0] = (uint8_t)(10 * (i + 1)); sc[i].state[0] = LJ_FADE; }
    cue.number = 1; cue.slot[0].seq = 1; cue.slot[0].enabled = 1; cue.slot[0].trig = LJ_TRIG_MANUAL;
    cue2 = cue; cue2.number = 2;
    lj_engine_bind(e, show);
    lj_engine_bind_profile(e, 0, &s_dim);
    r = lj_engine_runner(e, LJ_RUN_SLOT0);

    lj_t_load(e, &cue, &seq);
    lj_t_cmd(e, LJ_CMD_CHASE, LJ_RUN_SLOT0, 1, 1, 0);          /* Chase Man on slot 1, chase fade on */
    lj_t_cmd(e, LJ_CMD_TIME, 0, 50, 0, 0);                     /* cue time 50 %: must not touch the chase period (F10) */
    for (i = 0; i < 4; i++) { lj_engine_tick(e, now); now += T_TICK; }
    lj_t_cmd(e, LJ_CMD_CHASE_TAP, LJ_RUN_SLOT0, 0, 0, 100000);  /* the two taps, stamped by the UI at 1 ms */
    lj_engine_tick(e, now); now += T_TICK;                     /* t = 100000 */
    lj_engine_tick(e, now); now += T_TICK;                     /* t = 125000 */
    lj_t_cmd(e, LJ_CMD_CHASE_TAP, LJ_RUN_SLOT0, 0, 0, t_tap + 400);   /* stamped 130400, posted before the tick at 150000; 30.4 ms rounds to 30 ms */
    lj_engine_tick(e, now); now += T_TICK;                     /* t = 150000 */
    lj_engine_view(e, &st);
    CHECK(st.chase_tap_us == 30000 && st.chase_ms == 30 && r->chase_us == 30000);
    CHECK(r->scene_due_us == t_tap + 400 + 30000 && r->scene == 0);   /* re-armed at the tap stamp + period, no advance by the tap itself */
    t_tap += 400;
    /* every boundary t_tap + 30 ms * n is seen on the first tick at or after it; the grid never drifts */
    last_scene = r->scene; due = t_tap + 30000;
    for (i = 0; i < 80; i++) {
        uint64_t t = now;
        lj_engine_tick(e, t); now += T_TICK;
        while (due <= t) {                                     /* boundaries passed by this tick */
            advances++; due += 30000;
        }
        if (r->scene != last_scene) {
            k = (r->scene - last_scene + T_SCENES) % T_SCENES;  /* scenes advanced this tick (one boundary or two) */
            CHECK(k >= 1 && k <= 2);
            last_scene = r->scene;
        }
        CHECK(r->scene_due_us == due);                         /* absolute, drift-free (E10): exact to the microsecond */
        CHECK(r->fade_dur_us == 0);                            /* chase fade := 30 ms <= LJ_SNAP_MS: snaps (rig M3), not 2000 ms */
    }
    CHECK(r->scene == (uint16_t)(advances % T_SCENES));        /* every boundary advanced exactly one scene */
    CHECK(advances == (int)((now - T_TICK - t_tap) / 30000));
    /* the cue fader did not scale it, and a new cue keeps the global period (03 s5.5) */
    lj_t_load(e, &cue2, &seq);
    lj_t_cmd(e, LJ_CMD_CHASE, LJ_RUN_SLOT0, 1, 1, 0);
    lj_engine_tick(e, now);
    CHECK(r->chase_us == 30000 && r->scene_due_us == now + 30000);
    now += T_TICK;
    /* two manual trigs 120 ms apart set the period too (F10: any manual trig button or hotkey) */
    { uint64_t t1 = now - 1000;
      lj_t_cmd(e, LJ_CMD_TRIG, LJ_RUN_SLOT0, 0xFF, 0, t1);
      lj_engine_tick(e, now); now += 5 * T_TICK;
      lj_t_cmd(e, LJ_CMD_TRIG, LJ_RUN_SLOT0, 0xFF, 0, t1 + 120000);
      lj_engine_tick(e, now); now += T_TICK;
      lj_engine_view(e, &st);
      CHECK(st.chase_tap_us == 120000 && r->chase_us == 120000);
      CHECK(r->fade_dur_us == 120000 && r->scene_due_us == t1 + 240000); }   /* fade := chase period (LJ_CHASE_FADE_IS_PERIOD), not scaled */
    lj_t_cmd(e, LJ_CMD_PREFS, LJ_PREF_DISABLE_CHASE_FADE, 1, 0, 0);
    lj_t_cmd(e, LJ_CMD_TRIG, LJ_RUN_SLOT0, 0xFF, 0, 0);
    lj_engine_tick(e, now); now += T_TICK;
    CHECK(r->fade_dur_us == 0);                                /* [CUE] DisableChaseFade=1: chased channels snap */
    /* a gap above LJ_CHASE_MAX_MS is a new first tap, not a period */
    lj_t_cmd(e, LJ_CMD_CHASE_TAP, LJ_RUN_ALL, 0, 0, now);
    lj_engine_tick(e, now); now += 61000000;
    lj_engine_view(e, &st); k = (int)st.chase_tap_us;
    lj_t_cmd(e, LJ_CMD_CHASE, LJ_RUN_SLOT0, 0, 0, 0);          /* chase off: the 61 s gap is not a catch-up run */
    lj_t_cmd(e, LJ_CMD_CHASE_TAP, LJ_RUN_ALL, 0, 0, now);
    lj_engine_tick(e, now);
    lj_engine_view(e, &st);
    CHECK(st.chase_tap_us == (uint32_t)k && st.chase_tap_us % 1000 == 0);

    lj_engine_bind(e, NULL); lj_engine_destroy(e); free(show);
    printf("test_chase: %d advances, %d failure(s)\n", advances, fails);
    return fails ? 1 : 0;
}

static int s_held_lines;
static void lj_t_sink(void *ud, const char *line) { (void)ud; if (strstr(line, "lock: held")) s_held_lines++; }

int lj_test_lock_hold(int argc, char **argv);
int lj_test_lock_hold(int argc, char **argv)
{
    enum { T_FOOT = 20, T_FIX = 100, T_SEQS = 4 * LJ_CUE_SLOTS + LJ_BG_SLOTS, T_STATICS = 20, T_TICKS = 120 };
    lj_show *show = lj_t_show(T_FIX, T_FOOT);
    lj_engine *e = lj_engine_create();
    lj_sequence *seq = (lj_sequence *)calloc(T_SEQS, sizeof *seq);
    lj_scene *sc = (lj_scene *)calloc(T_SEQS * 2, sizeof *sc);
    lj_cue *cue = (lj_cue *)calloc(4, sizeof *cue);
    lj_static_slot *stat = (lj_static_slot *)calloc(T_STATICS, sizeof *stat);
    lj_scene *ssc = (lj_scene *)calloc(T_STATICS, sizeof *ssc);
    lj_bgcue bg;
    lj_cmd cmd;
    uint32_t max_us = 0;
    int i, k, ch, fail = 0;
    (void)argc; (void)argv;
    if (!show || !e || !seq || !sc || !cue || !stat || !ssc) { printf("FAIL alloc\n"); fail = 1; goto out; }
    for (i = 0; i < T_SEQS; i++) {
        seq[i].number = (uint32_t)i + 1; seq[i].n = 2; seq[i].scenes = &sc[2 * i];
        for (k = 0; k < 2; k++) {
            sc[2 * i + k].scene_ms = 500 + 37 * (uint32_t)i; sc[2 * i + k].fade_ms = 400;
            for (ch = 0; ch < LJ_CHANS; ch++) { sc[2 * i + k].val[ch] = (uint8_t)(k ? 255 - ch - i : ch + i); sc[2 * i + k].state[ch] = LJ_FADE; }
        }
    }
    memset(&bg, 0, sizeof bg); bg.number = 1;
    for (k = 0; k < LJ_BG_SLOTS; k++) { bg.slot[k].seq = (uint32_t)(4 * LJ_CUE_SLOTS + k + 1); bg.slot[k].enabled = 1; }
    for (i = 0; i < 4; i++) {
        cue[i].number = (uint32_t)i + 1; cue[i].cue_time_pct = 100; cue[i].bg_cue = 1;
        for (k = 0; k < LJ_CUE_SLOTS; k++) { cue[i].slot[k].seq = (uint32_t)(i * LJ_CUE_SLOTS + k + 1); cue[i].slot[k].enabled = 1; cue[i].slot[k].dir = (uint8_t)(k % 4); }
    }
    for (i = 0; i < T_STATICS; i++) {
        stat[i].kind = LJ_STATIC_RECORDED; stat[i].level_pct = 80; stat[i].scene = &ssc[i];
        for (ch = i * 100; ch < i * 100 + 100 && ch < LJ_CHANS; ch++) { ssc[i].val[ch] = 128; ssc[i].state[ch] = LJ_SNAP; }
    }
    for (ch = 0; ch < LJ_CHANS; ch++) show->patch[ch].htp_mask = 1u << (ch % LJ_HTP_GROUPS);
    lj_lock();
    lj_engine_bind(e, show);
    for (i = 0; i < T_FIX; i++) lj_engine_bind_profile(e, i, &s_dim);
    for (i = 0; i < T_STATICS; i++) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_STATIC_ACTIVATE; cmd.a = (uint32_t)i; cmd.b = 0xFFFF; cmd.c = 0xFF; cmd.st = &stat[i]; lj_engine_cmd(e, &cmd); }
    lj_unlock();
    lj_log_set_sink(lj_t_sink, NULL);
    for (i = 0; i < T_TICKS; i++) {
        uint64_t t0, t1;
        lj_lock();
        if (i % 30 == 0) {                                     /* the UI pins and posts the next of the 4 cues, BG latched */
            const lj_cue *c = &cue[(i / 30) % 4];
            memset(&cmd, 0, sizeof cmd);
            cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = c->number; cmd.cue = c; cmd.bg = &bg;
            for (k = 0; k < LJ_CUE_SLOTS; k++) cmd.seq[k] = &seq[c->slot[k].seq - 1];
            for (k = 0; k < LJ_BG_SLOTS; k++) cmd.bgseq[k] = &seq[bg.slot[k].seq - 1];
            lj_engine_cmd(e, &cmd);
        }
        t0 = lj_clock_us();
        lj_engine_tick(e, (uint64_t)i * LJ_TICK_US_DEFAULT);
        t1 = lj_clock_us();
        lj_unlock();
        if (i > 0 && t1 - t0 > max_us) max_us = (uint32_t)(t1 - t0);   /* the first tick warms the caches */
    }
    lj_log_set_sink(NULL, NULL);
    { lj_engine_status st; lj_engine_view(e, &st);
      if (!st.statics_any || !st.bg_any || st.cue_number != 4 || st.slot_status[11] != LJ_SLOT_RUNNING) { printf("FAIL load: statics %u bg %u cue %u\n", st.statics_any, st.bg_any, st.cue_number); fail = 1; } }
    printf("test_lock_hold: max tick under the lock %u us (limit %d us = tick / 4), %d \"lock: held\" line(s)\n", (unsigned)max_us, LJ_LOCK_HOLD_TEST_US, s_held_lines);
#if !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
    if (max_us >= (uint32_t)LJ_LOCK_HOLD_TEST_US) { printf("FAIL lock hold %u us >= %d us\n", (unsigned)max_us, LJ_LOCK_HOLD_TEST_US); fail = 1; }
#endif
out:
    if (e) lj_engine_bind(e, NULL);
    lj_engine_destroy(e);
    free(seq); free(sc); free(cue); free(stat); free(ssc); free(show);
    return fail;
}
