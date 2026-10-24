/* test_intensity_rule.c - `lj4evr_tests test_intensity_rule`: lj_seq_intensity_rule (lj_runner.h) against
 * Martin's 12-cell table of 03 s5.9 (seq_intensities.lj4r carries the same numbers through the engine):
 * S.I. 30/40/50 % = 77/102/128; programmed 100/50/30/10 % = 255/128/77/26; Default, HTP, LoTP per fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_runner.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_t_rule(int mode, int fixture, uint8_t *out)
{
    /* rows = slots 1..3 (Seq #1..#3), columns = fixtures 1..4; 0 = not programmed (Off) */
    static const uint8_t prog[3][4] = { { 0, 255, 128, 26 }, { 255, 0, 0, 0 }, { 77, 128, 0, 128 } };
    static const uint8_t si[3] = { 77, 102, 128 };
    uint8_t active[LJ_CUE_SLOTS], programmed[LJ_CUE_SLOTS], faded[LJ_CUE_SLOTS], sis[LJ_CUE_SLOTS];
    int k;
    memset(active, 0, sizeof active); memset(programmed, 0, sizeof programmed); memset(faded, 0, sizeof faded); memset(sis, LJ_SEQINT_UNASSIGNED, sizeof sis);
    for (k = 0; k < 3; k++) { active[k] = prog[k][fixture] != 0; programmed[k] = faded[k] = prog[k][fixture]; sis[k] = si[k]; }
    return lj_seq_intensity_rule(mode, active, programmed, faded, sis, out);
}

/* The same table through the engine (lj_merge calls the rule): 4 dimmers, slots 1..3, SEQ_INT 30/40/50 %, one tick per mode. */
static void lj_t_engine_table(void)
{
    static const uint8_t prog[3][4] = { { 0, 255, 128, 26 }, { 255, 0, 0, 0 }, { 77, 128, 0, 128 } };
    static const uint8_t si[3] = { 77, 102, 128 };
    static const uint8_t want[3][4] = { { 39, 64, 39, 64 }, { 102, 77, 39, 64 }, { 39, 64, 39, 8 } };   /* Default, HTP, LoTP */
    static lj_profile dim;
    lj_show *show = (lj_show *)calloc(1, sizeof *show);
    lj_engine *e = lj_engine_create();
    lj_sequence seq[3]; lj_scene sc[3]; lj_cue cue; lj_cmd c;
    int k, f, m;
    if (!show || !e) { fails++; free(show); lj_engine_destroy(e); return; }
    memset(&dim, 0, sizeof dim); dim.footprint = 1; dim.channel_class[0] = LJ_CH_INTENSITY; dim.use_gm = 1; dim.intensity.enabled = 1; dim.intensity.max = 255;
    memset(seq, 0, sizeof seq); memset(sc, 0, sizeof sc); memset(&cue, 0, sizeof cue);
    for (f = 0; f < 4; f++) { show->fixture[f].kind = LJ_FX_GS2; show->fixture[f].link = 1; show->fixture[f].address = (uint16_t)(f + 1); }
    lj_engine_bind(e, show);
    for (f = 0; f < 4; f++) lj_engine_bind_profile(e, f, &dim);
    cue.number = 1;
    for (k = 0; k < 3; k++) {
        seq[k].number = (uint32_t)k + 1; seq[k].n = 1; seq[k].scenes = &sc[k]; sc[k].scene_ms = 1000; sc[k].fade_ms = 50;
        for (f = 0; f < 4; f++) if (prog[k][f]) { sc[k].val[f] = prog[k][f]; sc[k].state[f] = LJ_SNAP; }
        cue.slot[k].seq = (uint32_t)k + 1; cue.slot[k].enabled = 1;
    }
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_LOAD_CUE; c.a = 1; c.cue = &cue; for (k = 0; k < 3; k++) c.seq[k] = &seq[k];
    lj_engine_cmd(e, &c);
    for (k = 0; k < 3; k++) { memset(&c, 0, sizeof c); c.kind = LJ_CMD_SEQ_INT; c.a = (uint32_t)k + 1; c.b = si[k]; lj_engine_cmd(e, &c); }
    for (m = 0; m < 3; m++) {
        const lj_frame *fr;
        memset(&c, 0, sizeof c); c.kind = LJ_CMD_SEQ_INT; c.a = 0; c.b = (uint32_t)m; lj_engine_cmd(e, &c);   /* LJ_SEQINT_DEFAULT / HTP / LOTP */
        lj_engine_tick(e, (uint64_t)m * LJ_TICK_US_DEFAULT);
        fr = lj_engine_frame(e);
        for (f = 0; f < 4; f++) if (fr->merged[f] != want[m][f] || fr->layer_of[f] != LJ_L_CUE) { fails++; printf("FAIL engine mode %d fixture %d: %u want %u\n", m, f + 1, fr->merged[f], want[m][f]); }
    }
    lj_engine_bind(e, NULL); lj_engine_destroy(e); free(show);
}

int lj_test_intensity_rule(int argc, char **argv);
int lj_test_intensity_rule(int argc, char **argv)
{
    uint8_t out; int win;
    (void)argc; (void)argv;
    /* Default: the highest slot with an active intensity wins, scaled by its S.I. */
    win = lj_t_rule(LJ_SEQINT_DEFAULT, 0, &out); CHECK(win == 2 && out == 39);     /* F1: 30 % x 50 % */
    win = lj_t_rule(LJ_SEQINT_DEFAULT, 1, &out); CHECK(win == 2 && out == 64);     /* F2: 50 % x 50 % */
    win = lj_t_rule(LJ_SEQINT_DEFAULT, 2, &out); CHECK(win == 0 && out == 39);     /* F3: 50 % x 30 % */
    win = lj_t_rule(LJ_SEQINT_DEFAULT, 3, &out); CHECK(win == 2 && out == 64);     /* F4: 50 % x 50 % */
    /* HTP: the highest programmed x S.I. wins */
    win = lj_t_rule(LJ_SEQINT_HTP, 0, &out); CHECK(win == 1 && out == 102);        /* F1: 100 % x 40 % */
    win = lj_t_rule(LJ_SEQINT_HTP, 1, &out); CHECK(win == 0 && out == 77);         /* F2: 100 % x 30 % */
    win = lj_t_rule(LJ_SEQINT_HTP, 2, &out); CHECK(win == 0 && out == 39);         /* F3: 50 % x 30 % */
    win = lj_t_rule(LJ_SEQINT_HTP, 3, &out); CHECK(win == 2 && out == 64);         /* F4: 50 % x 50 % */
    /* LoTP: the lowest */
    win = lj_t_rule(LJ_SEQINT_LOTP, 0, &out); CHECK(win == 2 && out == 39);        /* F1: 30 % x 50 % */
    win = lj_t_rule(LJ_SEQINT_LOTP, 1, &out); CHECK(win == 2 && out == 64);        /* F2: 50 % x 50 % */
    win = lj_t_rule(LJ_SEQINT_LOTP, 2, &out); CHECK(win == 0 && out == 39);        /* F3: 50 % x 30 % */
    win = lj_t_rule(LJ_SEQINT_LOTP, 3, &out); CHECK(win == 0 && out == 8);         /* F4: 10 % x 30 % */
    /* edge: no active slot */
    { uint8_t z[LJ_CUE_SLOTS]; memset(z, 0, sizeof z); CHECK(lj_seq_intensity_rule(LJ_SEQINT_HTP, z, z, z, z, &out) == -1); }
    /* an unassigned S.I. counts as 100 % (LJ_SEQINT_UNASSIGNED) */
    { uint8_t a[LJ_CUE_SLOTS], p[LJ_CUE_SLOTS], s[LJ_CUE_SLOTS];
      memset(a, 0, sizeof a); memset(p, 0, sizeof p); memset(s, LJ_SEQINT_UNASSIGNED, sizeof s);
      a[4] = 1; p[4] = 200;
      CHECK(lj_seq_intensity_rule(LJ_SEQINT_DEFAULT, a, p, p, s, &out) == 4 && out == 200); }
    lj_t_engine_table();
    printf("test_intensity_rule: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
