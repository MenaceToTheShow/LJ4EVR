/* test_engine_w2.c - `lj4evr_tests test_engine_w2`: the W2 engine kinds through lj_engine_tick, one block each:
 * the layer stack cue < flash < current < statics < BG < smoke (design 3.4); BG_ON and the view's BG fields;
 * transparent cues Use / Force / Ignore (03 s5.8, RN 2.2 b4); flash latch (03 s5.10); Smoke Control rank 7 on SMOKE /
 * HAZE / HAZE_FAN only (11 C12, ME-25); DEFAULT_OUT, GLOBAL_PATCH and HTP_GROUPS table swaps (S6 invert, S5 max);
 * SNAPSHOT_REQ; the blind buffer with [PREFS] KeepSeqBlind both ways (LORD_ENGINE-13); cue time control (03 s5.6);
 * Trig on Load + Scenes/Trig; LOOP_OPTS; ME-2 ownership for Link to cue (pending_link_cue handed out once, the
 * slot stopped, the record freed by the UI only after the engine was handed a replacement); ME-83 (movement-macro
 * index below the delay). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "engine/lj_movemacro.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { T_CH = 8 };                                             /* fixture 1: 8 channels at 1:1..1:8; offsets 5/6/7 = SMOKE / HAZE / HAZE_FAN */
static lj_profile s_prof;
static uint64_t s_now;

static lj_cmd *lj_t_c(int kind, uint32_t a, uint32_t b, uint32_t c)
{
    static lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = c;
    return &cmd;
}
static const lj_frame *lj_t_tick(lj_engine *e) { lj_engine_tick(e, s_now); s_now += LJ_TICK_US_DEFAULT; return lj_engine_frame(e); }

/* a sequence of n scenes; scene i sets channel ch to v0 + i (Snap) */
static void lj_t_seq(lj_sequence *q, lj_scene *sc, uint32_t number, int n, int ch, int v0, uint32_t scene_ms)
{
    int i;
    memset(q, 0, sizeof *q); memset(sc, 0, sizeof *sc * (size_t)n);
    q->number = number; q->n = (uint16_t)n; q->scenes = sc;
    for (i = 0; i < n; i++) { sc[i].scene_ms = scene_ms; sc[i].fade_ms = 50; sc[i].val[ch] = (uint8_t)(v0 + i); sc[i].state[ch] = LJ_SNAP; }
}

static void lj_t_load(lj_engine *e, const lj_cue *cue, const lj_sequence *const *seqs, int edit)
{
    lj_cmd *c = lj_t_c(LJ_CMD_LOAD_CUE, cue->number, (uint32_t)edit, 0);
    int k;
    c->cue = cue;
    for (k = 0; k < LJ_CUE_SLOTS; k++) c->seq[k] = seqs ? seqs[k] : NULL;
    lj_engine_cmd(e, c);
}

static void lj_t_layers(lj_engine *e)
{
    static lj_sequence qc, qf, qcur, qb[2]; static lj_scene sc_c, sc_f, sc_cur, sc_b[2], sc_s, sc_smoke;
    static lj_cue cue; static lj_bgcue bg; static lj_static_slot st;
    const lj_sequence *seqs[LJ_CUE_SLOTS] = { &qc };
    const lj_frame *f;
    lj_engine_status v;
    lj_cmd *c;
    lj_t_seq(&qc, &sc_c, 1, 1, 0, 10, 1000); sc_c.val[5] = 3; sc_c.state[5] = LJ_SNAP; sc_c.val[1] = 1; sc_c.state[1] = LJ_SNAP;
    lj_t_seq(&qf, &sc_f, 2, 1, 0, 20, 1000);
    lj_t_seq(&qcur, &sc_cur, 3, 1, 0, 30, 1000);
    lj_t_seq(&qb[0], &sc_b[0], 4, 1, 0, 50, 1000);
    lj_t_seq(&qb[1], &sc_b[1], 5, 1, 0, 60, 1000);
    memset(&cue, 0, sizeof cue); cue.number = 1; cue.slot[0].seq = 1; cue.slot[0].enabled = 1;
    lj_t_load(e, &cue, seqs, 0);
    f = lj_t_tick(e); CHECK(f->merged[0] == 10 && f->layer_of[0] == LJ_L_CUE);
    c = lj_t_c(LJ_CMD_FLASH, 0, 1, 0); c->seq[0] = &qf; lj_engine_cmd(e, c);
    f = lj_t_tick(e); CHECK(f->merged[0] == 20 && f->layer_of[0] == LJ_L_FLASH);          /* flash above the cue */
    c = lj_t_c(LJ_CMD_CURRENT_SEQ, 0, 0, 0); c->seq[0] = &qcur; lj_engine_cmd(e, c);
    f = lj_t_tick(e); CHECK(f->merged[0] == 30 && f->layer_of[0] == LJ_L_CURRENT);        /* current above the flash */
    memset(&st, 0, sizeof st); memset(&sc_s, 0, sizeof sc_s);
    st.kind = LJ_STATIC_RECORDED; st.level_pct = 100; st.scene = &sc_s; sc_s.val[0] = 40; sc_s.state[0] = LJ_SNAP;
    c = lj_t_c(LJ_CMD_STATIC_ACTIVATE, 0, 0xFFFF, 0xFF); c->st = &st; lj_engine_cmd(e, c);
    f = lj_t_tick(e); CHECK(f->merged[0] == 40 && f->layer_of[0] == LJ_L_STATICS);        /* statics above current */
    memset(&bg, 0, sizeof bg); bg.number = 7; bg.slot[0].seq = 4; bg.slot[1].seq = 5;     /* slot enabled flags 0: LJ_BG_RESTORE_ONOFF 0 loads them on */
    c = lj_t_c(LJ_CMD_LOAD_BG, 7, 0, 0); c->bg = &bg; c->bgseq[0] = &qb[0]; c->bgseq[1] = &qb[1]; lj_engine_cmd(e, c);
    f = lj_t_tick(e); CHECK(f->merged[0] == 60 && f->layer_of[0] == LJ_L_BGCUE);          /* BG above statics; BG slot 2 above slot 1 */
    lj_engine_view(e, &v); CHECK(v.bg_number == 7 && v.bg_enabled[0] && v.bg_enabled[1] && v.bg_any);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_BG_ON, LJ_RUN_BG0 + 1, 0, 0));
    f = lj_t_tick(e); CHECK(f->merged[0] == 50);                                            /* BGCue Toggle slot 2 off */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_BG_ON, LJ_RUN_ALL, 0, 0));
    f = lj_t_tick(e); CHECK(f->merged[0] == 40);                                            /* All Slots Off: statics show */
    lj_engine_view(e, &v); CHECK(!v.bg_any && !v.bg_enabled[0] && v.bg_number == 7);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_BG_ON, LJ_RUN_ALL, 1, 0));
    /* rank 7: the smoke button over everything on the SMOKE channel only */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_BUTTON, 1, 0));
    f = lj_t_tick(e); CHECK(f->merged[5] == 255 && f->layer_of[5] == LJ_L_SMOKE && f->merged[0] == 60 && f->merged[1] == 1);
    lj_engine_view(e, &v); CHECK(v.smoke_active);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_BUTTON, 0, 0));
    f = lj_t_tick(e); CHECK(f->merged[5] == 3 && f->layer_of[5] == LJ_L_CUE);              /* released: the scene value shows */
    (void)sc_smoke;
    /* clean up: release everything this block started */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_LOAD_BG, 0, 0, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_STATIC_DEACTIVATE, LJ_RUN_ALL, 0, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_CURRENT_SEQ, 0, 0, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_FLASH, 0, 0, 0));
    lj_t_load(e, &cue, NULL, 0); lj_engine_cmd(e, lj_t_c(LJ_CMD_LOAD_CUE, 0, 0, 0));
    f = lj_t_tick(e); CHECK(f->merged[0] == 0 && f->layer_of[0] == LJ_L_DEFAULT);
}

static void lj_t_smoke_timer(lj_engine *e)
{
    const lj_frame *f;
    lj_engine_status v;
    int i, on = 0, first_off = -1, again = -1;
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_LEVEL, 128, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_DURATION_MS, 1000, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_CYCLE_MS, 1000, 0));                   /* clamped to the 5 s minimum (04 s12) */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_TIMER, 1, 0));
    for (i = 0; i < 240; i++) {                                /* 6 s: smoke 0..1 s, off 1..5 s, smoke again at 5 s */
        f = lj_t_tick(e);
        if (i == 0) CHECK(f->merged[5] == 128 && f->layer_of[5] == LJ_L_SMOKE);            /* the timer uses the Level fader */
        if (f->merged[5] == 128) { if (first_off >= 0 && again < 0) again = i; on++; } else if (first_off < 0) first_off = i;
    }
    CHECK(first_off == 40 && again == 200 && on == 80);
    lj_engine_view(e, &v); CHECK(v.smoke_timer);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_TIMER, 0, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_BUTTON, 1, 100));                      /* toggle 100 ms: the release is ignored */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_BUTTON, 0, 100));
    f = lj_t_tick(e); CHECK(f->merged[5] == 255);
    for (i = 0; i < 4; i++) f = lj_t_tick(e);
    CHECK(f->merged[5] == 0);                                  /* 100 ms later it stops by itself */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_HAZE_LEVEL, 40, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_FAN_LEVEL, 300, 0));
    f = lj_t_tick(e);
    CHECK(f->merged[6] == 40 && f->merged[7] == 255 && f->layer_of[6] == LJ_L_SMOKE && f->merged[0] == 0 && f->merged[5] == 0);   /* > 0 overrides; ME-25: other classes untouched */
    lj_engine_view(e, &v); CHECK(v.haze_level == 40 && v.fan_level == 255 && !v.smoke_active);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_HAZE_LEVEL, 0, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_SMOKE, LJ_SMOKE_FAN_LEVEL, 0, 0));
    f = lj_t_tick(e); CHECK(f->layer_of[6] == LJ_L_DEFAULT && f->layer_of[7] == LJ_L_DEFAULT);
}

static void lj_t_transparent(lj_engine *e)
{
    static lj_sequence q[3]; static lj_scene sc[3]; static lj_cue c1, c2, c3;
    const lj_sequence *s1[LJ_CUE_SLOTS] = { &q[0], &q[1] }, *s2[LJ_CUE_SLOTS] = { &q[2] };
    const lj_frame *f;
    lj_t_seq(&q[0], &sc[0], 1, 1, 0, 10, 1000); lj_t_seq(&q[1], &sc[1], 2, 1, 1, 20, 1000); lj_t_seq(&q[2], &sc[2], 3, 1, 0, 30, 1000);
    memset(&c1, 0, sizeof c1); c1.number = 1; c1.slot[0].seq = 1; c1.slot[0].enabled = 1; c1.slot[1].seq = 2; c1.slot[1].enabled = 1;
    memset(&c2, 0, sizeof c2); c2.number = 2; c2.transparent = 1; c2.slot[0].seq = 3; c2.slot[0].enabled = 1;
    c3 = c2; c3.number = 3; c3.transparent = 0;
    /* Use: the saved flag; slot 2 carried over keeps its runner, its off state included (RN 2.2 b4) */
    lj_t_load(e, &c1, s1, 0);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_ENABLE, LJ_RUN_SLOT0 + 1, 0, 0));
    f = lj_t_tick(e); CHECK(f->merged[0] == 10 && f->merged[1] == 0);
    lj_t_load(e, &c2, s2, 0);
    f = lj_t_tick(e); CHECK(f->merged[0] == 30 && f->merged[1] == 0 && lj_engine_runner(e, LJ_RUN_SLOT0 + 1)->seq == &q[1] && !lj_engine_runner(e, LJ_RUN_SLOT0 + 1)->enabled);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_ENABLE, LJ_RUN_SLOT0 + 1, 1, 0));
    f = lj_t_tick(e); CHECK(f->merged[1] == 20);
    /* Ignore: a transparent cue loads as a normal one, slot 2 is cleared */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_TRANSPARENT, 0, LJ_TRANSPARENT_IGNORE, 0));
    lj_t_load(e, &c2, s2, 0);
    f = lj_t_tick(e); CHECK(f->merged[0] == 30 && f->merged[1] == 0 && lj_engine_runner(e, LJ_RUN_SLOT0 + 1)->seq_no == 0);
    /* Force: a normal cue loads transparently */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_TRANSPARENT, 0, LJ_TRANSPARENT_FORCE, 0));
    lj_t_load(e, &c1, s1, 0); lj_t_load(e, &c3, s2, 0);
    f = lj_t_tick(e); CHECK(f->merged[0] == 30 && f->merged[1] == 20);
    { lj_engine_status v; lj_engine_view(e, &v); CHECK(v.transparent_mode == LJ_TRANSPARENT_FORCE && v.cue_number == 3); }
    lj_engine_cmd(e, lj_t_c(LJ_CMD_TRANSPARENT, 0, LJ_TRANSPARENT_USE, 0));
    lj_engine_cmd(e, lj_t_c(LJ_CMD_LOAD_CUE, 0, 0, 0));
    lj_t_tick(e);
}

static void lj_t_flash_latch(lj_engine *e)
{
    static lj_sequence q; static lj_scene sc;
    const lj_frame *f;
    lj_engine_status v;
    lj_cmd *c;
    lj_t_seq(&q, &sc, 9, 1, 0, 99, 1000);
    c = lj_t_c(LJ_CMD_FLASH, 0, 1, 1); c->seq[0] = &q; lj_engine_cmd(e, c);   /* press, latched */
    c = lj_t_c(LJ_CMD_FLASH, 0, 0, 1); c->seq[0] = &q; lj_engine_cmd(e, c);   /* key release: stays */
    f = lj_t_tick(e); lj_engine_view(e, &v);
    CHECK(f->merged[0] == 99 && v.flash_latched && v.flash_seq == 9);
    c = lj_t_c(LJ_CMD_FLASH, 0, 1, 1); c->seq[0] = &q; lj_engine_cmd(e, c);   /* flashed again: released */
    f = lj_t_tick(e); lj_engine_view(e, &v);
    CHECK(f->merged[0] == 0 && !v.flash_latched && v.flash_seq == 0);
    c = lj_t_c(LJ_CMD_FLASH, 0, 1, 1); c->seq[0] = &q; lj_engine_cmd(e, c);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_FLASH, 0, 0, 0));                          /* Release latched Flash */
    f = lj_t_tick(e); CHECK(f->merged[0] == 0);
    c = lj_t_c(LJ_CMD_FLASH, 0, 1, 0); c->seq[0] = &q; lj_engine_cmd(e, c);   /* held */
    f = lj_t_tick(e); lj_engine_view(e, &v); CHECK(f->merged[0] == 99 && !v.flash_latched);
    c = lj_t_c(LJ_CMD_FLASH, 0, 0, 0); c->seq[0] = &q; lj_engine_cmd(e, c);   /* released */
    f = lj_t_tick(e); CHECK(f->merged[0] == 0);
}

static void lj_t_tables(lj_engine *e, lj_show *show)
{
    static uint8_t def2[LJ_CHANS]; static lj_patch_ch patch2[LJ_CHANS];
    static lj_frame snap;
    const lj_frame *f;
    lj_engine_status v;
    lj_cmd *c;
    memset(def2, 0, sizeof def2); def2[2] = 33;
    c = lj_t_c(LJ_CMD_DEFAULT_OUT, 0, 0, 0); c->default_out = def2; lj_engine_cmd(e, c);
    f = lj_t_tick(e); CHECK(f->merged[2] == 33 && f->layer_of[2] == LJ_L_DEFAULT && f->out[0][2] == 33);
    memcpy(patch2, show->patch, sizeof patch2); patch2[2].invert = 1;
    c = lj_t_c(LJ_CMD_GLOBAL_PATCH, 0, 0, 0); c->patch = patch2; lj_engine_cmd(e, c);
    f = lj_t_tick(e); CHECK(f->merged[2] == 33 && f->out[0][2] == 222);                   /* S6 from the swapped table */
    patch2[3].htp_mask = 1;
    c = lj_t_c(LJ_CMD_HTP_GROUPS, 0, 0, 0); c->patch = patch2; lj_engine_cmd(e, c);
    lj_engine_cmd(e, lj_t_c(LJ_CMD_HTP_GROUP, 1, 90, 0));
    f = lj_t_tick(e); CHECK(f->out[0][3] == 90 && f->out[0][4] == 0);                     /* S5: max(v, g1) on the member only */
    c = lj_t_c(LJ_CMD_SNAPSHOT_REQ, LJ_SNAPSHOT_OUT, 0, 0); c->snapshot = &snap; lj_engine_cmd(e, c);
    f = lj_t_tick(e); lj_engine_view(e, &v);
    CHECK(v.snapshot_tick == f->tick_no && snap.tick_no == f->tick_no && !memcmp(snap.out, f->out, sizeof snap.out) && snap.out[0][3] == 90);
    f = lj_t_tick(e); lj_engine_view(e, &v); CHECK(v.snapshot_tick == f->tick_no - 1);    /* served once */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_HTP_GROUP, 1, 0, 0));
    c = lj_t_c(LJ_CMD_GLOBAL_PATCH, 0, 0, 0); c->patch = show->patch; lj_engine_cmd(e, c);
    c = lj_t_c(LJ_CMD_DEFAULT_OUT, 0, 0, 0); lj_engine_cmd(e, c);                         /* NULL = the show's own */
    f = lj_t_tick(e); CHECK(f->out[0][2] == 0 && f->out[0][3] == 0);
}

static void lj_t_blind(lj_engine *e)
{
    static lj_sequence q; static lj_scene sc[2];
    const lj_frame *f;
    lj_cmd *c;
    int keep;
    for (keep = 1; keep >= 0; keep--) {
        lj_t_seq(&q, sc, 4, 2, 0, 50, 1000); sc[1].val[0] = 99;
        lj_engine_cmd(e, lj_t_c(LJ_CMD_PREFS, LJ_PREF_KEEP_SEQ_BLIND, (uint32_t)keep, 0));
        c = lj_t_c(LJ_CMD_CURRENT_SEQ, 0, 0, 0); c->seq[0] = &q; lj_engine_cmd(e, c);
        f = lj_t_tick(e); CHECK(f->merged[0] == 50);
        c = lj_t_c(LJ_CMD_CURRENT_SEQ, 0, 1, 0); c->seq[0] = &q; lj_engine_cmd(e, c);    /* blind on */
        f = lj_t_tick(e); CHECK(f->merged[0] == (keep ? 50 : 0));                          /* the buffer keeps contributing by default */
        c = lj_t_c(LJ_CMD_CURRENT_SEQ, 1, 1, 0); c->seq[0] = &q; lj_engine_cmd(e, c);    /* edit scene 2 while blind */
        f = lj_t_tick(e); CHECK(f->merged[0] == (keep ? 50 : 0));                          /* live edits never reach the output */
        c = lj_t_c(LJ_CMD_CURRENT_SEQ, 1, 0, 0); c->seq[0] = &q; lj_engine_cmd(e, c);    /* blind off */
        f = lj_t_tick(e); CHECK(f->merged[0] == 99);
        lj_engine_cmd(e, lj_t_c(LJ_CMD_CURRENT_SEQ, 0, 0, 0));
    }
    lj_engine_cmd(e, lj_t_c(LJ_CMD_PREFS, LJ_PREF_KEEP_SEQ_BLIND, LJ_KEEP_CONTENTS_BLIND, 0));
    lj_t_tick(e);
}

static void lj_t_cue_time_and_loops(lj_engine *e)
{
    static lj_sequence q; static lj_scene sc[4]; static lj_cue cue;
    const lj_sequence *seqs[LJ_CUE_SLOTS] = { &q, &q };
    const lj_runner *r0 = lj_engine_runner(e, LJ_RUN_SLOT0), *r1 = lj_engine_runner(e, LJ_RUN_SLOT0 + 1);
    lj_cue_slot ls;
    int i;
    lj_t_seq(&q, sc, 5, 4, 0, 1, 1000);
    memset(&cue, 0, sizeof cue); cue.number = 5; cue.cue_time_pct = 200;
    cue.slot[0].seq = 5; cue.slot[0].enabled = 1;                                          /* slot 1 at CUE: the cue fader (200 %) */
    cue.slot[1].seq = 5; cue.slot[1].enabled = 1; cue.slot[1].time_pct = 50;               /* slot 2's own fader */
    lj_t_load(e, &cue, seqs, 0);
    lj_t_tick(e);
    CHECK(r0->scene_due_us - r0->fade_t0_us == 2000000 && r1->scene_due_us - r1->fade_t0_us == 500000);   /* 03 s5.6: min(60000, ms * pct / 100) */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_CUE_TIME, 0, 0, 0));                                    /* the cue fader to CUE (100 %) */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_CUE_TIME, 2, 5000, 0));                                 /* clamped to 999 */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_TRIG, LJ_RUN_ALL, 0xFF, 0));
    lj_t_tick(e);
    CHECK(r0->scene_due_us - r0->fade_t0_us == 1000000 && r1->time_pct == LJ_TIME_PCT_MAX && r1->scene_due_us - r1->fade_t0_us == 9990000);
    /* LOOP_OPTS: reverse, limits 1..4, the running scene kept */
    memset(&ls, 0, sizeof ls); ls.dir = LJ_DIR_REV; ls.first = 1; ls.last = 4;
    { lj_cmd *c = lj_t_c(LJ_CMD_LOOP_OPTS, LJ_RUN_SLOT0, 0, 0); uint16_t was = r0->scene; c->slot = &ls; lj_engine_cmd(e, c);
      lj_t_tick(e); CHECK(r0->scene == was && r0->dir == LJ_DIR_REV);
      lj_engine_cmd(e, lj_t_c(LJ_CMD_TRIG, LJ_RUN_SLOT0, 0xFF, 0));
      lj_t_tick(e); CHECK(r0->scene == (uint16_t)(was ? was - 1 : 3)); }
    /* Trig on Load: manual slot, Scenes/Trig 3: the load is the trig, two extra scenes run auto-timed, then it waits */
    memset(&cue, 0, sizeof cue); cue.number = 6;
    cue.slot[0].seq = 5; cue.slot[0].enabled = 1; cue.slot[0].trig = LJ_TRIG_MANUAL; cue.slot[0].scenes_per_trig = 3; cue.slot[0].trig_on_load = 1;
    lj_t_load(e, &cue, seqs, 0);
    for (i = 0; i < 200; i++) lj_t_tick(e);                    /* 5 s */
    CHECK(r0->scene == 2 && r0->spt_left == 0);
    cue.slot[0].trig_on_load = 0;
    lj_t_load(e, &cue, seqs, 0);
    for (i = 0; i < 200; i++) lj_t_tick(e);
    CHECK(r0->scene == 0);                                     /* without it the manual slot holds its first scene */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_LOAD_CUE, 0, 0, 0));
    lj_t_tick(e);
}

static void lj_t_link_ownership(lj_engine *e)
{
    lj_sequence *q = (lj_sequence *)calloc(1, sizeof *q);
    lj_scene *sc = (lj_scene *)calloc(2, sizeof *sc);
    static lj_cue cue;
    lj_engine_status v;
    const lj_sequence *seqs[LJ_CUE_SLOTS] = { NULL };
    int i, seen = 0, edit;
    if (!q || !sc) { fails++; free(q); free(sc); return; }
    for (edit = 0; edit <= 1; edit++) {
        lj_t_seq(q, sc, 6, 2, 0, 70, 100);
        memset(&cue, 0, sizeof cue); cue.number = 8;
        cue.slot[0].seq = 6; cue.slot[0].enabled = 1; cue.slot[0].loops = 1; cue.slot[0].end_action = LJ_END_LINK; cue.slot[0].link_cue = 9;
        seqs[0] = q;
        lj_t_load(e, &cue, seqs, edit);
        seen = 0;
        for (i = 0; i < 20; i++) { lj_t_tick(e); lj_engine_view(e, &v); if (v.pending_link_cue) { CHECK(v.pending_link_cue == 9); seen++; } }
        CHECK(seen == (edit ? 0 : 1));                         /* handed out once; Edit Cue never fires the link */
        CHECK(lj_engine_runner(e, LJ_RUN_SLOT0)->stopped && (int)lj_engine_runner(e, LJ_RUN_SLOT0)->link == !edit);
    }
    lj_t_load(e, &cue, NULL, 0);                               /* the UI hands the engine a NULL for the record ... */
    lj_t_tick(e);
    CHECK(lj_engine_runner(e, LJ_RUN_SLOT0)->seq == NULL);
    free(sc); free(q);                                         /* ... and frees it only after that tick (ASan proves rule 6) */
    lj_engine_cmd(e, lj_t_c(LJ_CMD_LOAD_CUE, 0, 0, 0));
    lj_t_tick(e);
}

static void lj_t_movemacro_me83(void)
{
    lj_movemacro m; lj_mm_state a, b; int32_t pa, ta, pb, tb;
    memset(&m, 0, sizeof m);
    m.type = 6; m.step = 512; m.amp_pan = m.amp_tilt = 127; m.fstate[0] = LJ_FADE;
    a.idx_pan = a.idx_tilt = 0; m.delay = 1;                   /* idx 0 - delay 16 = -16: wraps to 4080, stepped to 3584 */
    lj_movemacro_offset(&a, &m, 100, &pa, &ta);
    b.idx_pan = b.idx_tilt = 4080u << 16; m.delay = 0;         /* the same index without a delay */
    lj_movemacro_offset(&b, &m, 100, &pb, &tb);
    CHECK(pa == pb && ta == tb);
}

int lj_test_engine_w2(int argc, char **argv);
int lj_test_engine_w2(int argc, char **argv)
{
    lj_show *show = (lj_show *)calloc(1, sizeof *show);
    lj_engine *e = lj_engine_create();
    int i;
    (void)argc; (void)argv;
    if (!show || !e) { printf("FAIL alloc\n"); free(show); lj_engine_destroy(e); return 1; }
    memset(&s_prof, 0, sizeof s_prof);
    s_prof.footprint = T_CH; s_prof.use_gm = 1; s_prof.intensity.enabled = 1; s_prof.intensity.max = 255;
    for (i = 0; i < 5; i++) s_prof.channel_class[i] = LJ_CH_INTENSITY;
    s_prof.channel_class[5] = LJ_CH_SMOKE; s_prof.channel_class[6] = LJ_CH_HAZE; s_prof.channel_class[7] = LJ_CH_HAZE_FAN;
    show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].link = 1; show->fixture[0].address = 1;
    lj_engine_bind(e, show);
    lj_engine_bind_profile(e, 0, &s_prof);
    lj_t_layers(e);
    lj_t_smoke_timer(e);
    lj_t_transparent(e);
    lj_t_flash_latch(e);
    lj_t_tables(e, show);
    lj_t_blind(e);
    lj_t_cue_time_and_loops(e);
    lj_t_link_ownership(e);
    lj_t_movemacro_me83();
    lj_engine_bind(e, NULL); lj_engine_destroy(e); free(show);
    printf("test_engine_w2: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
