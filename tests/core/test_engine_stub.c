/* test_engine_stub.c - the W0 engine contract surface, re-pointed at the real W1
 * engine: command array fills at 256 and drops with a count (design 2.3 rule 1),
 * an unbound engine publishes an all-zero frame at rank 0, pending_* fields
 * clear once handed out; and "dump-frames [path]": 400 ticks of a small fixed
 * show (two dimmers and a 16-bit head fading through three scenes under GM 200
 * with a movement macro) to frames.bin (merged[2048] + out[4][512] + tick_no as
 * 8 LE bytes per tick) for the CI determinism cmp job (design 9.3). ME-60: the
 * signed sine table is never right-shifted; the one table use here is the
 * unsigned form (lj_sin4096[i] + 32768) >> 8. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "engine/lj_trig_gen.h"
#include "model/lj_model.h"
#include "core/lj_log.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int s_drop_lines;
static void lj_count_drops(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "command array full")) s_drop_lines++;
}

int lj_test_engine_stub(int argc, char **argv);
int lj_test_engine_stub(int argc, char **argv)
{
    lj_engine *e = lj_engine_create();
    lj_engine_status st;
    lj_cmd cmd;
    const lj_frame *f;
    int i, ok = 0, ch;
    (void)argc; (void)argv;
    CHECK(e != NULL);
    if (!e) return 1;
    lj_engine_bind(e, NULL);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_GO;
    lj_log_set_sink(lj_count_drops, NULL);
    for (i = 0; i < 300; i++) ok += lj_engine_cmd(e, &cmd) == 0;
    lj_log_set_sink(NULL, NULL);
    CHECK(ok == LJ_ENGINE_CMD_CAP && s_drop_lines == 300 - LJ_ENGINE_CMD_CAP);   /* 44 logged drops, design 2.3 */
    lj_engine_view(e, &st);
    CHECK(st.drops == 300 - LJ_ENGINE_CMD_CAP);
    lj_engine_tick(e, 0);                                        /* drains */
    CHECK(lj_engine_cmd(e, &cmd) == 0);
    f = lj_engine_frame(e);
    CHECK(f->tick_no == 1);
    for (i = 1; i < 3; i++) lj_engine_tick(e, (uint64_t)i * LJ_TICK_US_DEFAULT);
    CHECK(f->tick_no == 3);
    for (ch = 0, ok = 1; ch < LJ_CHANS; ch++)                    /* unbound: rank 0 of nothing = 0 everywhere */
        if (f->merged[ch] || f->out[ch / LJ_LINK_CH][ch % LJ_LINK_CH] || f->layer_of[ch]) ok = 0;
    CHECK(ok);
    CHECK((uint8_t)((lj_sin4096[1024] + 32768) >> 8) == 255 && (uint8_t)((lj_sin4096[3072] + 32768) >> 8) == 0);   /* ME-60: the unsigned form */
    lj_engine_destroy(e);
    lj_engine_destroy(NULL);
    printf("test_engine_stub: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}

/* ---- dump-frames: a fixed show through the real engine ---- */
static lj_profile s_dimmer, s_head;

static void lj_dump_show(lj_show *show, lj_sequence *seq, lj_cue *cue)
{
    int i;
    memset(&s_dimmer, 0, sizeof s_dimmer);
    s_dimmer.footprint = 1; s_dimmer.channel_class[0] = LJ_CH_INTENSITY; s_dimmer.use_gm = 1; s_dimmer.intensity.enabled = 1; s_dimmer.intensity.max = 255;
    memset(&s_head, 0, sizeof s_head);
    s_head.footprint = 6; s_head.pt16 = 1; s_head.pan_ofs = 0; s_head.tilt_ofs = 2;
    s_head.channel_class[0] = s_head.channel_class[1] = LJ_CH_PAN; s_head.channel_class[2] = s_head.channel_class[3] = LJ_CH_TILT;
    s_head.channel_class[4] = LJ_CH_INTENSITY; s_head.channel_class[5] = LJ_CH_COLOR; s_head.use_gm = 1; s_head.intensity.enabled = 1; s_head.intensity.offset = 4; s_head.intensity.max = 255;
    show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].link = 1; show->fixture[0].address = 1;
    show->fixture[1].kind = LJ_FX_BUILTIN; show->fixture[1].link = 1; show->fixture[1].address = 2;
    show->fixture[2].kind = LJ_FX_BUILTIN; show->fixture[2].link = 2; show->fixture[2].address = 1;
    show->default_out[5] = 64;
    seq->number = 1; seq->n = 3; seq->scenes = (lj_scene *)calloc(3, sizeof(lj_scene));
    for (i = 0; i < 3; i++) {
        lj_scene *s = &seq->scenes[i];
        s->scene_ms = 3000; s->fade_ms = i == 2 ? 50 : 2500;
        s->val[0] = (uint8_t)(i * 120); s->state[0] = LJ_FADE;
        s->val[1] = (uint8_t)(255 - i * 100); s->state[1] = i == 1 ? LJ_SNAP : LJ_FADE;
        s->val[512] = (uint8_t)(64 * i); s->val[513] = (uint8_t)(200 - i * 37); s->state[512] = s->state[513] = LJ_FADE;   /* pan 16-bit */
        s->val[514] = 128; s->val[515] = 0; s->state[514] = s->state[515] = LJ_SNAP;                                       /* tilt */
        s->val[516] = 255; s->state[516] = LJ_FADE;
        s->macro[2].type = 1; s->macro[2].amp_pan = 60; s->macro[2].amp_tilt = 40; s->macro[2].cycle_pan = 30; s->macro[2].fstate[0] = LJ_SNAP;   /* ellipse, 3 s */
    }
    cue->number = 1; cue->cue_time_pct = 100; cue->slot[0].seq = 1; cue->slot[0].enabled = 1;
}

int lj_test_dump_frames(int argc, char **argv);
int lj_test_dump_frames(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "frames.bin";
    lj_engine *e = lj_engine_create();
    lj_show *show = (lj_show *)calloc(1, sizeof *show);   /* test code: no model bodies needed */
    lj_sequence *seq = (lj_sequence *)calloc(1, sizeof *seq);
    lj_cue *cue = (lj_cue *)calloc(1, sizeof *cue);
    lj_cmd cmd;
    const lj_frame *f;
    FILE *out;
    int i, k, ok = 1;
    if (!e) return 1;
    lj_dump_show(show, seq, cue);
    lj_engine_bind(e, show);
    lj_engine_bind_profile(e, 0, &s_dimmer); lj_engine_bind_profile(e, 1, &s_dimmer); lj_engine_bind_profile(e, 2, &s_head);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 1; cmd.cue = cue; cmd.seq[0] = seq; lj_engine_cmd(e, &cmd);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_SET_MASTER; cmd.a = 0; cmd.b = 200; lj_engine_cmd(e, &cmd);
    out = fopen(path, "wb");
    if (!out) { printf("dump-frames: cannot write %s\n", path); lj_engine_destroy(e); return 1; }
    f = lj_engine_frame(e);
    for (i = 0; i < 400 && ok; i++) {
        uint8_t tn[8];
        lj_engine_tick(e, (uint64_t)i * LJ_TICK_US_DEFAULT);
        for (k = 0; k < 8; k++) tn[k] = (uint8_t)(f->tick_no >> (8 * k));
        ok = fwrite(f->merged, 1, sizeof f->merged, out) == sizeof f->merged
          && fwrite(f->out, 1, sizeof f->out, out) == sizeof f->out
          && fwrite(tn, 1, 8, out) == 8;
    }
    ok = fclose(out) == 0 && ok;
    lj_engine_bind(e, NULL);
    lj_engine_destroy(e);
    free(seq->scenes); free(seq); free(cue); free(show);
    printf("dump-frames: %d tick(s) -> %s%s\n", i, path, ok ? "" : " (write failed)");
    return ok ? 0 : 1;
}
