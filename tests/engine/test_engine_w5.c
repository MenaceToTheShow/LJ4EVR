/* test_engine_w5.c - `lj4evr_tests test_engine_w5`: the W5 engine seams (docs/w5-contracts.md seam 2 and 4).
 *  - LJ_CMD_DMX_IN copies the UI's 512 bytes into post.dmx_in[] of its input link at the drain; NULL = zeros; a link
 *    outside 1..LJ_LINKS is ignored; LJ_CMD_OVERWRITE_ENABLE gates S1 (lj_post_s1_patch_src Absolute / HTP), except a
 *    W6 "Ignore DMX In Enable" channel (ign_en), which takes DMX-in with the gate off too.
 *  - LJ_CMD_TRIG b = LJ_TRIG_AUDIO_EVENT advances only the slots in audio trig mode, one scene per audio_trigs_per_trig
 *    trigs (Audio-Trigs / Trig, 05 s9.1), and never an auto or manual slot. */
#include <stdio.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_post.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint64_t s_now;
static lj_cmd *cmd(int kind, uint32_t a, uint32_t b)
{
    static lj_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = (uint8_t)kind; c.a = a; c.b = b;
    return &c;
}
static void tick(lj_engine *e) { lj_engine_tick(e, s_now); s_now += LJ_TICK_US_DEFAULT; }

static void t_dmx_in(lj_engine *e)
{
    static lj_patch_ch patch[LJ_CHANS];
    static uint8_t in[LJ_LINK_CH];
    const lj_post_ctx *p = lj_engine_post_ctx(e);
    lj_cmd *c;
    memset(patch, 0, sizeof patch);
    patch[9].src_kind = LJ_SRC_DMX_IN; patch[9].src_link = 1; patch[9].src_ch = 5; patch[9].in_mode = LJ_IN_ABSOLUTE;   /* 1:10 <- in 1:5 */
    patch[10].src_kind = LJ_SRC_DMX_IN; patch[10].src_link = 1; patch[10].src_ch = 6; patch[10].in_mode = LJ_IN_HTP;     /* 1:11 <- in 1:6 HTP */
    patch[11] = patch[9]; patch[11].ign_en = 1;                                                                           /* W6: 1:12 <- in 1:5, Ignore DMX In Enable */
    c = cmd(LJ_CMD_GLOBAL_PATCH, 0, 0); c->patch = patch; lj_engine_cmd(e, c);
    memset(in, 0, sizeof in); in[4] = 200; in[5] = 30;
    c = cmd(LJ_CMD_DMX_IN, 1, 0); c->dmx_in = in; lj_engine_cmd(e, c);
    tick(e);
    CHECK(p->dmx_in[4] == 200 && p->dmx_in[5] == 30);
    CHECK(lj_post_s1_patch_src(p, 1, 9, 77) == 77);                   /* Overwrite Enable off: the cue value stands */
    CHECK(lj_post_s1_patch_src(p, 1, 11, 77) == 200);                 /* W6: an Ignore channel takes DMX-in with the gate off */
    lj_engine_cmd(e, cmd(LJ_CMD_OVERWRITE_ENABLE, 0, 1)); tick(e);
    CHECK(p->overwrite_enable == 1);
    CHECK(lj_post_s1_patch_src(p, 1, 9, 77) == 200);                  /* Absolute */
    CHECK(lj_post_s1_patch_src(p, 1, 11, 77) == 200);                 /* W6: gate on, Ignore the same */
    CHECK(lj_post_s1_patch_src(p, 1, 10, 77) == 77);                  /* HTP: 77 > 30 */
    in[4] = 1;                                                        /* the UI changes its buffer: nothing moves until the next DMX_IN */
    tick(e); CHECK(p->dmx_in[4] == 200);
    c = cmd(LJ_CMD_DMX_IN, 1, 0); c->dmx_in = in; lj_engine_cmd(e, c); tick(e);
    CHECK(p->dmx_in[4] == 1);
    c = cmd(LJ_CMD_DMX_IN, 1, 0); lj_engine_cmd(e, c); tick(e);       /* NULL = zeros (the loss rule) */
    CHECK(p->dmx_in[4] == 0 && p->dmx_in[5] == 0);
    c = cmd(LJ_CMD_DMX_IN, LJ_LINKS + 1, 0); c->dmx_in = in; lj_engine_cmd(e, c); tick(e);   /* out of range: ignored */
    c = cmd(LJ_CMD_DMX_IN, 0, 0); c->dmx_in = in; lj_engine_cmd(e, c); tick(e);
    CHECK(p->dmx_in[4] == 0);
    c = cmd(LJ_CMD_DMX_IN, 2, 0); c->dmx_in = in; lj_engine_cmd(e, c); tick(e);              /* input link 2 = address codes 512.. */
    CHECK(p->dmx_in[LJ_LINK_CH + 4] == 1 && p->dmx_in[4] == 0);
    lj_engine_cmd(e, cmd(LJ_CMD_OVERWRITE_ENABLE, 0, 0)); tick(e);
    CHECK(lj_post_s1_patch_src(p, 1, 9, 77) == 77);
}

static void t_audio(lj_engine *e)
{
    static lj_sequence q[3]; static lj_scene sc[3][4]; static lj_cue cue;
    const lj_runner *r0, *r1, *r2;
    lj_cmd *c;
    int i, k;
    for (k = 0; k < 3; k++) {
        memset(&q[k], 0, sizeof q[k]); memset(sc[k], 0, sizeof sc[k]);
        q[k].number = (uint32_t)(k + 1); q[k].n = 4; q[k].scenes = sc[k];
        for (i = 0; i < 4; i++) { sc[k][i].scene_ms = 60000; sc[k][i].fade_ms = 0; sc[k][i].val[k] = (uint8_t)(10 + i); sc[k][i].state[k] = LJ_SNAP; }
    }
    memset(&cue, 0, sizeof cue); cue.number = 1;
    cue.slot[0].seq = 1; cue.slot[0].enabled = 1; cue.slot[0].trig = LJ_TRIG_AUDIO; cue.slot[0].audio_trigs_per_trig = 2;
    cue.slot[1].seq = 2; cue.slot[1].enabled = 1; cue.slot[1].trig = LJ_TRIG_AUDIO; cue.slot[1].audio_trigs_per_trig = 1;
    cue.slot[2].seq = 3; cue.slot[2].enabled = 1; cue.slot[2].trig = LJ_TRIG_MANUAL;
    c = cmd(LJ_CMD_LOAD_CUE, 1, 0); c->cue = &cue; c->seq[0] = &q[0]; c->seq[1] = &q[1]; c->seq[2] = &q[2]; lj_engine_cmd(e, c);
    tick(e);
    r0 = lj_engine_runner(e, LJ_RUN_SLOT0); r1 = lj_engine_runner(e, LJ_RUN_SLOT0 + 1); r2 = lj_engine_runner(e, LJ_RUN_SLOT0 + 2);
    CHECK(r0->scene == 0 && r1->scene == 0 && r2->scene == 0);
    for (k = 1; k <= 4; k++) {
        lj_engine_cmd(e, cmd(LJ_CMD_TRIG, LJ_RUN_ALL, LJ_TRIG_AUDIO_EVENT)); tick(e);
        CHECK(r0->scene == (uint16_t)(k / 2));                       /* 2 audio trigs per scene: advances on every second trig */
        CHECK(r1->scene == (uint16_t)(k % 4));                       /* 1 per scene, wrapping over 4 scenes */
        CHECK(r2->scene == 0);                                       /* a manual slot ignores audio trigs */
    }
    lj_engine_cmd(e, cmd(LJ_CMD_TRIG, LJ_RUN_SLOT0, LJ_TRIG_AUDIO_EVENT)); tick(e);   /* a must be LJ_RUN_ALL: ignored */
    CHECK(r0->scene == 2 && r1->scene == 0);
    lj_engine_cmd(e, cmd(LJ_CMD_TRIG, LJ_RUN_SLOT0 + 2, 0xFF)); tick(e);              /* the manual trig path is untouched */
    CHECK(r2->scene == 1);
}

int lj_test_engine_w5(int argc, char **argv)
{
    lj_engine *e = lj_engine_create();
    (void)argc; (void)argv;
    if (!e) { printf("FAIL lj_engine_create\n"); return 1; }
    s_now = 1000000;
    t_dmx_in(e);
    t_audio(e);
    lj_engine_destroy(e);
    printf("test_engine_w5: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
