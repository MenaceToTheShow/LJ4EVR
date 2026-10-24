/* test_post.c - one runnable check per engine rule that the replay scenarios
 * also cover, without the harness: the S3-before-S2 order (11 s5.2 Example 3
 * and 6, ME-1), LJ_SWITCH_UNSCALED (ME-25), blackout after masters (rig M17),
 * HTP max before the patch clamp (rig M18), overwrite last (rig M19), S7
 * invert + patch (17 s4.3 example 2), a disabled offset (LORD_FIXTURES-11),
 * the layer ranks 1-6 with statics LTP, freeze clock (rig M10), interrupted
 * fade restart (rig M6), generic macro +-186 (14 sA.3), movement macro
 * ellipse (17 s7.3). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "model/lj_model.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct { lj_show *show; lj_engine *e; const lj_frame *f; lj_sequence seq[4]; lj_scene scenes[4][3]; lj_cue cue; lj_bgcue bg; lj_genmacro *gm; lj_static_slot st[2]; lj_scene stsc[2]; uint64_t now; } lj_t;

static lj_t *lj_t_begin(void)
{
    lj_t *t = (lj_t *)calloc(1, sizeof *t);
    int i, k;
    t->show = (lj_show *)calloc(1, sizeof *t->show);
    t->gm = (lj_genmacro *)calloc(1, sizeof *t->gm);
    t->e = lj_engine_create();
    t->f = lj_engine_frame(t->e);
    for (i = 0; i < 4; i++) { t->seq[i].number = (uint32_t)i + 1; t->seq[i].n = 1; t->seq[i].scenes = t->scenes[i]; for (k = 0; k < 3; k++) { t->scenes[i][k].scene_ms = 1000; t->scenes[i][k].fade_ms = 50; } }
    t->cue.number = 1; t->cue.cue_time_pct = 100;
    t->bg.number = 1;
    for (i = 0; i < 2; i++) { t->st[i].kind = LJ_STATIC_RECORDED; t->st[i].level_pct = 100; t->st[i].scene = &t->stsc[i]; }
    return t;
}

static void lj_t_fixture(lj_t *t, int i, const lj_profile *p, int link, int addr, int group)
{
    t->show->fixture[i].kind = LJ_FX_GS2; t->show->fixture[i].link = (uint8_t)link; t->show->fixture[i].address = (uint16_t)addr; t->show->fixture[i].intensity_group = (uint8_t)group;
    lj_engine_bind(t->e, t->show);
    lj_engine_bind_profile(t->e, i, p);
}

static void lj_t_ch(lj_t *t, int seq, int scene, int ch, int v, int state) { t->scenes[seq][scene].val[ch] = (uint8_t)v; t->scenes[seq][scene].state[ch] = (uint8_t)state; }

static void lj_t_cmd(lj_t *t, int kind, uint32_t a, uint32_t b, uint32_t c)
{
    lj_cmd cmd; memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = c;
    if (kind == LJ_CMD_LOAD_CUE) { int k; cmd.cue = &t->cue; for (k = 0; k < LJ_CUE_SLOTS; k++) if (t->cue.slot[k].seq) cmd.seq[k] = &t->seq[t->cue.slot[k].seq - 1]; }
    if (kind == LJ_CMD_LOAD_BG) { int k; cmd.bg = &t->bg; for (k = 0; k < LJ_BG_SLOTS; k++) if (t->bg.slot[k].seq) cmd.bgseq[k] = &t->seq[t->bg.slot[k].seq - 1]; }
    if (kind == LJ_CMD_LOAD_GENMACRO) cmd.gm = t->gm;
    if (kind == LJ_CMD_STATIC) cmd.st = &t->st[a];
    if (kind == LJ_CMD_FLASH || kind == LJ_CMD_CURRENT_SEQ) cmd.seq[0] = &t->seq[c];   /* c = sequence index here */
    lj_engine_cmd(t->e, &cmd);
}

static void lj_t_slot(lj_t *t, int slot, int seq) { t->cue.slot[slot - 1].seq = (uint32_t)seq; t->cue.slot[slot - 1].enabled = 1; }

static void lj_t_tick(lj_t *t, int n) { while (n-- > 0) { lj_engine_tick(t->e, t->now); t->now += LJ_TICK_US_DEFAULT; } }

static int lj_t_out(const lj_t *t, int ch) { return t->f->out[ch / LJ_LINK_CH][ch % LJ_LINK_CH]; }

static void lj_t_end(lj_t *t) { lj_engine_bind(t->e, NULL); lj_engine_destroy(t->e); free(t->gm); free(t->show); free(t); }

static lj_profile s_dim, s_ex3, s_ex6, s_sw, s_bo, s_dis, s_head;

static void lj_profiles_init(void)
{
    memset(&s_dim, 0, sizeof s_dim);
    s_dim.footprint = 1; s_dim.channel_class[0] = LJ_CH_INTENSITY; s_dim.use_gm = 1; s_dim.intensity.enabled = 1; s_dim.intensity.max = 255;
    s_ex3 = s_dim; s_ex3.invert_int = 1; s_ex3.invert_int_range = 1;                          /* Example 3: 0-255 inverted */
    s_ex6 = s_dim; s_ex6.invert_int = 1; s_ex6.invert_int_range = 1; s_ex6.limit_gm = 1; s_ex6.intensity.min = 100; s_ex6.intensity.max = 200;   /* Example 6 */
    memset(&s_sw, 0, sizeof s_sw); s_sw.footprint = 1; s_sw.channel_class[0] = LJ_CH_SWITCH;  /* ME-25 */
    s_bo = s_dim; s_bo.blackout.enabled = 1; s_bo.blackout.defined[0] = 1; s_bo.blackout.value[0] = 20;
    s_dis = s_dim; s_dis.footprint = 2; s_dis.channel_class[1] = LJ_CH_COLOR; s_dis.level_disabled[1] = 1;
    memset(&s_head, 0, sizeof s_head);
    s_head.footprint = 4; s_head.pt16 = 1; s_head.pan_ofs = 0; s_head.tilt_ofs = 2;
    s_head.channel_class[0] = s_head.channel_class[1] = LJ_CH_PAN; s_head.channel_class[2] = s_head.channel_class[3] = LJ_CH_TILT;
}

int lj_test_engine_post(int argc, char **argv);
int lj_test_engine_post(int argc, char **argv)
{
    lj_t *t;
    int k, v;
    (void)argc; (void)argv;
    lj_profiles_init();

    /* ---- S3 before S2: Example 3 and 6, SWITCH, blackout literal, HTP, overwrite, disabled offset ---- */
    t = lj_t_begin();
    lj_t_fixture(t, 0, &s_ex3, 1, 1, 0); lj_t_fixture(t, 1, &s_ex6, 1, 2, 0); lj_t_fixture(t, 2, &s_ex6, 1, 3, 0); lj_t_fixture(t, 3, &s_ex6, 1, 4, 0);
    lj_t_fixture(t, 4, &s_sw, 1, 5, 0); lj_t_fixture(t, 5, &s_bo, 1, 6, 0); lj_t_fixture(t, 6, &s_bo, 1, 7, 0); lj_t_fixture(t, 7, &s_dim, 1, 8, 0);
    lj_t_fixture(t, 8, &s_dim, 1, 9, 0); lj_t_fixture(t, 9, &s_dis, 1, 10, 0);
    t->show->fixture[6].ignore_bo = 1;
    t->show->patch[7].htp_mask = 1; t->show->patch[7].max = 200;
    t->show->default_out[10] = 5;
    lj_t_ch(t, 0, 0, 0, 255, LJ_SNAP);   /* Example 3: programmed 100 % */
    lj_t_ch(t, 0, 0, 1, 200, LJ_SNAP);   /* Example 6: 100 % = control value 200 */
    lj_t_ch(t, 0, 0, 2, 50, LJ_SNAP);    /* strobe zones */
    lj_t_ch(t, 0, 0, 3, 230, LJ_SNAP);
    lj_t_ch(t, 0, 0, 4, 200, LJ_SNAP);   /* switch */
    lj_t_ch(t, 0, 0, 5, 200, LJ_SNAP); lj_t_ch(t, 0, 0, 6, 200, LJ_SNAP);   /* blackout fixtures */
    lj_t_ch(t, 0, 0, 7, 100, LJ_SNAP);   /* HTP member */
    lj_t_ch(t, 0, 0, 8, 100, LJ_SNAP);   /* overwrite target */
    lj_t_ch(t, 0, 0, 9, 66, LJ_SNAP); lj_t_ch(t, 0, 0, 10, 77, LJ_SNAP);   /* fixture 9: offset 0 live, offset 1 disabled */
    lj_t_slot(t, 1, 1);
    lj_t_cmd(t, LJ_CMD_LOAD_CUE, 1, 0, 0);
    lj_t_tick(t, 1);
    CHECK(lj_t_out(t, 0) == 0 && lj_t_out(t, 1) == 100 && lj_t_out(t, 2) == 50 && lj_t_out(t, 3) == 230);   /* GM 100 % */
    CHECK(lj_t_out(t, 4) == 200 && lj_t_out(t, 7) == 100 && lj_t_out(t, 9) == 66 && t->f->layer_of[8] == LJ_L_CUE);
    CHECK(lj_t_out(t, 10) == 5 && t->f->layer_of[10] == LJ_L_DEFAULT);   /* LORD_FIXTURES-11: disabled offset silent, default_out shows */
    lj_t_cmd(t, LJ_CMD_SET_MASTER, 0, 128, 0);
    lj_t_tick(t, 1);
    CHECK(lj_t_out(t, 0) == 127);                              /* Example 3 at GM 50 %: scaled 128, then inverted (rig M21) */
    CHECK(lj_t_out(t, 1) == 150 && lj_t_out(t, 2) == 50 && lj_t_out(t, 3) == 230);   /* Example 6: 150, strobes untouched */
    CHECK(lj_t_out(t, 4) == 200);                              /* ME-25: SWITCH unscaled at GM 50 % */
    CHECK(lj_t_out(t, 5) == 100 && lj_t_out(t, 7) == 50 && lj_t_out(t, 8) == 50);
    lj_t_cmd(t, LJ_CMD_BLACKOUT, 0, 1, 0);
    lj_t_cmd(t, LJ_CMD_HTP_GROUP, 1, 255, 0);
    lj_t_cmd(t, LJ_CMD_OVERWRITE, 8, 200, 0);
    lj_t_tick(t, 1);
    CHECK(lj_t_out(t, 5) == 20 && lj_t_out(t, 6) == 100);     /* literal blackout value after masters; Ignore Blackout keeps the scaled value (rig M17) */
    CHECK(lj_t_out(t, 7) == 200);                              /* HTP 255 not master-scaled, then the patch max clamps (rig M18) */
    CHECK(lj_t_out(t, 8) == 200);                              /* overwrite last (rig M19) */
    lj_t_cmd(t, LJ_CMD_BLACKOUT, 0, 0, 0);
    lj_t_cmd(t, LJ_CMD_SET_MASTER, 0, 0, 0);
    lj_t_tick(t, 1);
    CHECK(lj_t_out(t, 0) == 255 && lj_t_out(t, 1) == 200 && lj_t_out(t, 4) == 0);   /* GM 0: Example 3 -> 255, Example 6 -> 200, SWITCH -> 0 */
    lj_t_end(t);

    /* ---- S7: 17 s4.3 example 2 (LSB first) with invert_pan ---- */
    t = lj_t_begin();
    s_head.pt_patch_on = 1; s_head.pt_patch[0] = 1; s_head.pt_patch[1] = 0; s_head.pt_patch[2] = 3; s_head.pt_patch[3] = 2;
    lj_t_fixture(t, 0, &s_head, 1, 1, 0);
    lj_t_ch(t, 0, 0, 0, 64, LJ_SNAP); lj_t_ch(t, 0, 0, 1, 128, LJ_SNAP); lj_t_ch(t, 0, 0, 2, 194, LJ_SNAP); lj_t_ch(t, 0, 0, 3, 224, LJ_SNAP);
    lj_t_slot(t, 1, 1);
    lj_t_cmd(t, LJ_CMD_LOAD_CUE, 1, 0, 0);
    lj_t_tick(t, 1);
    CHECK(lj_t_out(t, 1) == 64 && lj_t_out(t, 0) == 128 && lj_t_out(t, 3) == 194 && lj_t_out(t, 2) == 224);
    t->show->fixture[0].invert_pan = 1;                        /* 65535 - 16512 = 49023 = 191:127 */
    lj_t_tick(t, 1);
    CHECK(lj_t_out(t, 1) == 191 && lj_t_out(t, 0) == 127 && lj_t_out(t, 3) == 194);
    s_head.pt_patch_on = 0;
    lj_t_end(t);

    /* ---- layer ranks: slot 12 > slot 1, flash, current, statics LTP, bg, generic macro ---- */
    t = lj_t_begin();
    lj_t_fixture(t, 0, &s_dim, 1, 1, 0);
    lj_t_ch(t, 0, 0, 0, 10, LJ_SNAP); lj_t_ch(t, 1, 0, 0, 30, LJ_SNAP); lj_t_ch(t, 2, 0, 0, 40, LJ_SNAP); lj_t_ch(t, 3, 0, 0, 50, LJ_SNAP);
    lj_t_slot(t, 1, 1); lj_t_slot(t, 12, 2);
    lj_t_cmd(t, LJ_CMD_LOAD_CUE, 1, 0, 0);
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 30 && t->f->layer_of[0] == LJ_L_CUE);          /* slot 12 wins [03 s5.3] */
    lj_t_cmd(t, LJ_CMD_FLASH, 0, 1, 2);
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 40 && t->f->layer_of[0] == LJ_L_FLASH);
    lj_t_cmd(t, LJ_CMD_CURRENT_SEQ, 0, 0, 3);
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 50 && t->f->layer_of[0] == LJ_L_CURRENT);
    lj_t_cmd(t, LJ_CMD_CURRENT_SEQ, 0, 1, 3);                 /* blind: the buffer keeps contributing (LORD_ENGINE-13), live edits do not */
    t->scenes[3][0].val[0] = 55;
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 50);
    t->stsc[0].val[0] = 60; t->stsc[0].state[0] = LJ_SNAP; t->stsc[1].val[0] = 70; t->stsc[1].state[0] = LJ_SNAP;
    lj_t_cmd(t, LJ_CMD_STATIC, 1, 1, 0);
    lj_t_tick(t, 1);
    lj_t_cmd(t, LJ_CMD_STATIC, 0, 1, 0);                      /* activated later: wins (LTP) */
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 60 && t->f->layer_of[0] == LJ_L_STATICS);
    lj_t_cmd(t, LJ_CMD_STATIC, 0, 0, 0);                      /* release: back to the other entry */
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 70);
    t->scenes[1][0].val[0] = 80; t->bg.slot[2].seq = 2; t->bg.slot[2].enabled = 1;
    lj_t_cmd(t, LJ_CMD_LOAD_BG, 1, 0, 0);
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 80 && t->f->layer_of[0] == LJ_L_BGCUE);
    t->gm->ch[0].base_on = 1; t->gm->ch[0].base = 90; t->gm->ch[0].amp_pos = 1;   /* cycle 0: static at shape[0] = 0 */
    lj_t_cmd(t, LJ_CMD_LOAD_GENMACRO, 1, 0, 0);
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 90 && t->f->layer_of[0] == LJ_L_GENMACRO);
    lj_t_end(t);

    /* ---- generic macro +-186 on Sine (14 sA.3), 1 s cycle = 40 ticks ---- */
    t = lj_t_begin();
    lj_t_fixture(t, 0, &s_dim, 1, 1, 0); lj_t_fixture(t, 1, &s_dim, 1, 2, 0);
    t->gm->ch[0].base_on = 1; t->gm->ch[0].base = 0;   t->gm->ch[0].shape = 7; t->gm->ch[0].amp_pos = 186; t->gm->ch[0].cycle_ds = 10;
    t->gm->ch[1].base_on = 1; t->gm->ch[1].base = 255; t->gm->ch[1].shape = 7; t->gm->ch[1].amp_neg = 186; t->gm->ch[1].cycle_ds = 10;
    lj_t_cmd(t, LJ_CMD_LOAD_GENMACRO, 1, 0, 0);
    lj_t_tick(t, 11); CHECK(lj_t_out(t, 0) == 186 && lj_t_out(t, 1) == 255);   /* tick 10: index 2048, the Sine peak */
    lj_t_tick(t, 20); CHECK(lj_t_out(t, 0) == 0 && lj_t_out(t, 1) == 69);      /* tick 30: index 6144, the trough */
    lj_t_end(t);

    /* ---- freeze clock (rig M10) and interrupted fade restart (rig M6) ---- */
    t = lj_t_begin();
    lj_t_fixture(t, 0, &s_dim, 1, 1, 0);
    t->scenes[0][0].scene_ms = 10000; t->scenes[0][0].fade_ms = 2550; lj_t_ch(t, 0, 0, 0, 255, LJ_FADE);
    lj_t_slot(t, 1, 1);
    lj_t_cmd(t, LJ_CMD_LOAD_CUE, 1, 0, 0);
    lj_t_tick(t, 21); CHECK(lj_t_out(t, 0) == 50);             /* tick 20: 255 * 500 / 2550 = 50 */
    lj_t_cmd(t, LJ_CMD_FREEZE, 0, 1, 0);
    lj_t_tick(t, 20); CHECK(lj_t_out(t, 0) == 50);             /* held through tick 40 */
    lj_t_cmd(t, LJ_CMD_FREEZE, 0, 0, 0);
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 53);              /* tick 41: the fade resumes with 2050 ms left: 525 ms in -> 52.5 -> 53 */
    lj_t_end(t);
    t = lj_t_begin();
    lj_t_fixture(t, 0, &s_dim, 1, 1, 0);
    t->seq[0].n = 3;
    t->scenes[0][0].scene_ms = 1000;  t->scenes[0][0].fade_ms = 50;    lj_t_ch(t, 0, 0, 0, 0, LJ_SNAP);
    t->scenes[0][1].scene_ms = 5000;  t->scenes[0][1].fade_ms = 10000; lj_t_ch(t, 0, 1, 0, 255, LJ_FADE);
    t->scenes[0][2].scene_ms = 20000; t->scenes[0][2].fade_ms = 10000; lj_t_ch(t, 0, 2, 0, 0, LJ_FADE);
    lj_t_slot(t, 1, 1);
    lj_t_cmd(t, LJ_CMD_LOAD_CUE, 1, 0, 0);
    lj_t_tick(t, 240); CHECK(lj_t_out(t, 0) == 127);           /* tick 239 = 5975 ms: 4975 ms into the 10 s fade -> 126.9 */
    lj_t_tick(t, 1); CHECK(lj_t_out(t, 0) == 127);             /* tick 240 = 6 s: scene 3 entered from the cut value 127 (F6), step 0 */
    lj_t_tick(t, 200); CHECK(lj_t_out(t, 0) == 63);            /* tick 440 = 11 s: 127 - round(127 * 5/10) */
    lj_t_end(t);

    /* ---- movement macro: ellipse, amp 127, 3 s cycle on a homed head (17 s7.3) ---- */
    t = lj_t_begin();
    lj_t_fixture(t, 0, &s_head, 1, 1, 0);
    lj_t_ch(t, 0, 0, 0, 128, LJ_SNAP); lj_t_ch(t, 0, 0, 1, 0, LJ_SNAP); lj_t_ch(t, 0, 0, 2, 128, LJ_SNAP); lj_t_ch(t, 0, 0, 3, 0, LJ_SNAP);
    t->scenes[0][0].macro[0].type = 1; t->scenes[0][0].macro[0].amp_pan = 127; t->scenes[0][0].macro[0].amp_tilt = 127; t->scenes[0][0].macro[0].cycle_pan = 30; t->scenes[0][0].macro[0].fstate[0] = LJ_SNAP;
    lj_t_slot(t, 1, 1);
    lj_t_cmd(t, LJ_CMD_LOAD_CUE, 1, 0, 0);
    lj_t_tick(t, 1); v = lj_t_out(t, 0) * 256 + lj_t_out(t, 1); CHECK(v == 65535);          /* index 0: cos = 1 -> +32768, cut off at the edge */
    k = lj_t_out(t, 2) * 256 + lj_t_out(t, 3); CHECK(k == 32768);                            /* sin 0 */
    lj_t_tick(t, 30); v = lj_t_out(t, 0) * 256 + lj_t_out(t, 1); CHECK(v > 32768 - 100 && v < 32768 + 100);   /* quarter cycle: cos ~ 0 */
    k = lj_t_out(t, 2) * 256 + lj_t_out(t, 3); CHECK(k > 65535 - 100);                      /* sin ~ 1 */
    lj_t_tick(t, 30); v = lj_t_out(t, 0) * 256 + lj_t_out(t, 1); CHECK(v < 100);            /* half cycle: cos ~ -1 */
    lj_t_end(t);

    printf("test_engine_post: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
