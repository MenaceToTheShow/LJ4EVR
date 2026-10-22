/* test_intensity_rule.c - `lj4evr_tests test_intensity_rule`: lj_seq_intensity_rule (lj_runner.h) against
 * Martin's 12-cell table of 03 s5.9 (seq_intensities.lj4r carries the same numbers through the engine):
 * S.I. 30/40/50 % = 77/102/128; programmed 100/50/30/10 % = 255/128/77/26; Default, HTP, LoTP per fixture. */
#include <stdio.h>
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
    printf("test_intensity_rule: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
