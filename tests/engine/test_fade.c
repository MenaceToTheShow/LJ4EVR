/* test_fade.c - design 3.3 fade maths: the lj_fade_value table (0 -> 255 and
 * 255 -> 0 at 40 Hz over 2550 ms hit 3/128/255 and 252/127/0 at ticks
 * 1/51/102, ties, dur 0, now <= t0), then the same fade through the real engine
 * on a dimmer and on a 16-bit pan pair (ME-22: one 16-bit value per tick,
 * the LSB non-zero on intermediate ticks), the 0.05 s snap (11 F4) and the
 * scaled-fade snap test (LORD_ENGINE-11). No replay harness needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "model/lj_model.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* profile 0: a 1-channel dimmer; profile 1: a 16-bit pan/tilt head (pan MSB 0, LSB 1, tilt MSB 2, LSB 3) */
static lj_profile s_dimmer, s_head;

static void lj_profiles_init(void)
{
    memset(&s_dimmer, 0, sizeof s_dimmer);
    s_dimmer.footprint = 1; s_dimmer.channel_class[0] = LJ_CH_INTENSITY;
    s_dimmer.use_gm = 1; s_dimmer.intensity.enabled = 1; s_dimmer.intensity.max = 255;
    memset(&s_head, 0, sizeof s_head);
    s_head.footprint = 4; s_head.pt16 = 1; s_head.pan_ofs = 0; s_head.tilt_ofs = 2;
    s_head.channel_class[0] = s_head.channel_class[1] = LJ_CH_PAN; s_head.channel_class[2] = s_head.channel_class[3] = LJ_CH_TILT;
}

static lj_sequence *lj_seq_make(int n)
{
    lj_sequence *s = (lj_sequence *)calloc(1, sizeof *s);
    int i;
    s->number = 1; s->n = (uint16_t)n; s->scenes = (lj_scene *)calloc((size_t)n, sizeof(lj_scene));
    for (i = 0; i < n; i++) { s->scenes[i].scene_ms = 10000; s->scenes[i].fade_ms = 2550; }
    return s;
}

static int lj_run_fade(uint32_t fade_ms, uint16_t time_pct, int ticks, uint8_t out[][4], uint16_t pan_out[])
{
    lj_show *show = (lj_show *)calloc(1, sizeof *show);   /* test code: no model bodies needed */
    lj_engine *e = lj_engine_create();
    lj_sequence *seq = lj_seq_make(1);
    lj_cue *cue = (lj_cue *)calloc(1, sizeof *cue);
    lj_cmd cmd;
    const lj_frame *f;
    int k;
    show->fixture[0].kind = LJ_FX_GS2; show->fixture[0].link = 1; show->fixture[0].address = 1;    /* dimmer at 1:001 */
    show->fixture[1].kind = LJ_FX_GS2; show->fixture[1].link = 1; show->fixture[1].address = 2;    /* dimmer at 1:002 (255 -> 0) */
    show->fixture[2].kind = LJ_FX_GS2; show->fixture[2].link = 1; show->fixture[2].address = 10;   /* head at 1:010 */
    show->default_out[1] = 255;
    seq->scenes[0].fade_ms = fade_ms;
    seq->scenes[0].val[0] = 255; seq->scenes[0].state[0] = LJ_FADE;
    seq->scenes[0].val[1] = 0;   seq->scenes[0].state[1] = LJ_FADE;
    seq->scenes[0].val[9] = 255; seq->scenes[0].val[10] = 255; seq->scenes[0].state[9] = seq->scenes[0].state[10] = LJ_FADE;   /* pan 0 -> 65535 */
    cue->number = 1; cue->cue_time_pct = 100; cue->slot[0].seq = 1; cue->slot[0].enabled = 1; cue->slot[0].time_pct = time_pct;
    lj_engine_bind(e, show);
    lj_engine_bind_profile(e, 0, &s_dimmer); lj_engine_bind_profile(e, 1, &s_dimmer); lj_engine_bind_profile(e, 2, &s_head);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 1; cmd.cue = cue; cmd.seq[0] = seq;
    CHECK(lj_engine_cmd(e, &cmd) == 0);
    f = lj_engine_frame(e);
    for (k = 0; k < ticks; k++) {
        lj_engine_tick(e, (uint64_t)k * LJ_TICK_US_DEFAULT);
        out[k][0] = f->out[0][0]; out[k][1] = f->out[0][1]; out[k][2] = f->out[0][9]; out[k][3] = f->out[0][10];
        pan_out[k] = (uint16_t)(f->out[0][9] * 256 + f->out[0][10]);
    }
    lj_engine_destroy(e);
    free(seq->scenes); free(seq); free(cue); free(show);
    return 0;
}

int lj_test_engine_fade(int argc, char **argv);
int lj_test_engine_fade(int argc, char **argv)
{
    static uint8_t out[110][4];
    static uint16_t pan[110];
    const uint64_t T = LJ_TICK_US_DEFAULT, D = 2550000;
    int k, mono = 1, lsb_nonzero = 0;
    (void)argc; (void)argv;
    /* the table of 3.3 */
    CHECK(lj_fade_value(0, 255, 1 * T, 0, D) == 3);   CHECK(lj_fade_value(255, 0, 1 * T, 0, D) == 252);
    CHECK(lj_fade_value(0, 255, 51 * T, 0, D) == 128); CHECK(lj_fade_value(255, 0, 51 * T, 0, D) == 127);   /* tie 127.5: nearest, away from `from` (LJ_FADE_ROUND_NEAREST) */
    CHECK(lj_fade_value(0, 255, 102 * T, 0, D) == 255); CHECK(lj_fade_value(255, 0, 102 * T, 0, D) == 0);
    CHECK(lj_fade_value(0, 255, 500 * T, 0, D) == 255);       /* past the end */
    CHECK(lj_fade_value(10, 200, 5, 0, 0) == 200);            /* dur 0 = snap */
    CHECK(lj_fade_value(10, 200, 5, 10, 1000) == 10);         /* now < t0: from, no wrap */
    CHECK(lj_fade_value(10, 200, 10, 10, 1000) == 10);        /* now == t0 */
    CHECK(lj_fade_value(0, 65535, 1 * T, 0, D) == 643);       /* 16-bit: 642.5 -> 643 */
    CHECK(lj_fade_value(100, 101, 1, 0, 2) == 101);           /* tie on one step: away from `from` */
    CHECK(lj_fade_value(101, 100, 1, 0, 2) == 100);
    /* through the engine: fade_linear_40hz on the wire, pair16 as one value */
    lj_profiles_init();
    lj_run_fade(2550, 0, 105, out, pan);
    CHECK(out[0][0] == 0 && out[0][1] == 255);                /* trig tick: from (M5 fade step 0) */
    CHECK(out[1][0] == 3 && out[1][1] == 252);
    CHECK(out[51][0] == 128 && out[51][1] == 127);
    CHECK(out[102][0] == 255 && out[102][1] == 0);
    CHECK(out[104][0] == 255 && out[104][1] == 0);
    CHECK(pan[1] == 643 && pan[51] == 32768 && pan[102] == 65535);   /* ME-22: exact 16-bit value per tick */
    for (k = 1; k < 103; k++) { if (pan[k] < pan[k - 1]) mono = 0; if (out[k][3] != 0 && k < 102) lsb_nonzero++; }
    CHECK(mono && lsb_nonzero > 90);                          /* monotonic, the LSB alive on intermediate ticks */
    /* 11 F4: 50 ms is a snap; LORD_ENGINE-11: 500 ms at 5 % = 25 ms snaps too; 60 ms fades */
    lj_run_fade(50, 0, 3, out, pan);
    CHECK(out[0][0] == 255 && out[0][1] == 0 && pan[0] == 65535);
    lj_run_fade(500, 5, 3, out, pan);
    CHECK(out[0][0] == 255);
    lj_run_fade(60, 0, 4, out, pan);
    CHECK(out[0][0] == 0 && out[1][0] == 106 && out[2][0] == 213 && out[3][0] == 255);   /* 255 * 25/60 = 106.25, 212.5 -> 213 */
    printf("test_engine_fade: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
