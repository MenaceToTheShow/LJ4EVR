/* test_genmacro.c - `lj4evr_tests test_genmacro` (W3 genmacro track): the 66 regenerated shapes (count, names, the
 * 14 sA.6 range of each, the sine family against tests/vectors.h), the 14 sA.3 arithmetic of lj_genmacro_step (the
 * Sine / amplitude 186 example tick by tick, its -186..186 readout, the Amp+ / Amp- halves on their own LORD_ENGINE-14,
 * a negative amplitude mirroring, bounce ignoring the delay), lj_genmacro_advance (40 Hz index, rev, bounce), the
 * ME-34 override loader on synthetic Macros.Mac / UserMacros.Mac files (values used, every truncation refused), the
 * .CMA round trip incl. shape 0 and bounce with the untrusted-field rejections, and Auto Delay with Martin's examples
 * (14 sA.5). Temp files: test_genmacro_*.{Mac,CMA} in the cwd. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_genmacro.h"
#include "engine/lj_shapes_gen.h"
#include "model/lj_genmacro.h"
#include "model/lj_words.h"
#include "vectors.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 14 sA.6 table: min..max per record as decoded from Macros.Mac; 57 / 58 are our authored (x / 4096)^3 / ^6 peaks,
 * which miss Martin's saturated samples next to the swap (max |diff| 126 / 249, docs/generated-files.md). */
static const struct { int16_t lo, hi; } lj_gm_range[LJ_GM_SHAPES] = {
    {0,32640},{0,32640},{0,32640},{0,32640},{0,32640},{0,32640},{-32640,32640},{-32640,32640},{-32640,32640},{-32640,32640},
    {-32640,32640},{-32640,32640},{0,32640},{-32640,32640},{-32640,32640},{-32640,32640},{0,32640},{0,32640},{0,32640},{-32640,32640},
    {-32640,32640},{-32640,32640},{-32640,32640},{0,32668},{0,32640},{0,32640},{0,32654},{0,32640},{0,32632},{0,32631},
    {53,32640},{-32630,32631},{0,32640},{-32640,32640},{-32640,32640},{-32640,32640},{-32640,32640},{0,32636},{-32632,32632},{-32624,32624},
    {-32608,32608},{-32576,32576},{-32640,32640},{-32640,32640},{-32640,32640},{-32640,32640},{-32640,32640},{0,32640},{0,32640},{0,32640},
    {0,32640},{0,32640},{-32640,32640},{-32640,32640},{-32640,32640},{-32640,32640},{-32766,32755},{-32616,32640} /* note: -32640 */,{-32592,32592} /* note: +-32640 */,{-29647,32654},
    {-32640,32640},{0,32640},{-32640,32640},{-32640,32640},{0,32640},{-32640,32640} };

static void lj_test_shapes(void)
{
    const lj_gm_shapes *b = &lj_gm_shapes_builtin;
    int i, k;
    CHECK(b->n == LJ_GM_SHAPES && b->nb == LJ_GM_SHAPES && b->s);
    CHECK(!strcmp(b->name[0], "2*Sine") && !strcmp(b->name[7], "Sine") && !strcmp(b->name[19], "Pulse 8<>") && !strcmp(b->name[65], "Static 2"));
    CHECK(!strcmp(b->name[1], "2*Sine^2") && !strcmp(b->name[12], "2*Sine^2"));   /* the duplicate is kept [14 sA.6 (e)] */
    for (k = 0; k < LJ_GM_SAMPLES; k++) CHECK(b->s[1 * LJ_GM_SAMPLES + k] == b->s[12 * LJ_GM_SAMPLES + k]);
    for (i = 0; i < LJ_GM_SHAPES; i++) {
        int lo = 99999, hi = -99999;
        for (k = 0; k < LJ_GM_SAMPLES; k++) { int v = lj_genmacro_shape_in(NULL, i, k); if (v < lo) lo = v; if (v > hi) hi = v; }
        if (lo != lj_gm_range[i].lo || hi != lj_gm_range[i].hi) { fails++; printf("FAIL shape %d %s range %d..%d, want %d..%d\n", i, b->name[i], lo, hi, lj_gm_range[i].lo, lj_gm_range[i].hi); }
    }
    for (i = 0; i < (int)(sizeof lj_vec_gmacro_sine / sizeof lj_vec_gmacro_sine[0]); i++) {   /* round(32640 sin) from the generator */
        CHECK(lj_genmacro_shape_in(NULL, 0, lj_vec_gmacro_sine[i].k) == lj_vec_gmacro_sine[i].two_sine);
        CHECK(lj_genmacro_shape_in(&lj_gm_shapes_builtin, 7, lj_vec_gmacro_sine[i].k) == lj_vec_gmacro_sine[i].sine);
    }
    _Static_assert(LJ_VEC_GMACRO_NSAMP == LJ_GM_SAMPLES && LJ_VEC_GMACRO_SCALE == LJ_GM_FULL, "vectors.h and the shape table agree");
    CHECK(lj_genmacro_shape_in(NULL, 7, 2048 + 8192) == 32640 && lj_genmacro_shape_in(NULL, 7, -2048) == -32640);   /* k wraps mod 8192 */
    CHECK(lj_genmacro_shape_in(NULL, LJ_GM_SHAPES, 100) == 0 && lj_genmacro_shape_in(NULL, -1, 100) == 0);       /* past the table: silent */
    CHECK(lj_genmacro_shape_in(NULL, 42, 4096) == 32640 && lj_genmacro_shape_in(NULL, 42, 4097) == -32640);        /* Square: 4097 / 4095 [14 sA.6 (d)] */
}

static lj_gm_param lj_p(int shape, int base_on, int base, int ap, int an, int dir, int delay)
{
    lj_gm_param p;
    memset(&p, 0, sizeof p);
    p.shape = (uint8_t)shape; p.base_on = (uint8_t)base_on; p.base = (uint8_t)base; p.amp_pos = (int16_t)ap; p.amp_neg = (int16_t)an;
    p.cycle_ds = 10; p.dir = (uint8_t)dir; p.delay = (uint16_t)delay;
    return p;
}

/* 14 sA.3 written out again: delta = s * amp / 32640 nearest (half away from 0), Amp+ for s >= 0, Amp- for s < 0. */
static int lj_ref_delta(int s, int ap, int an)
{
    long a = s >= 0 ? ap : an, q = (long)s * a;
    long m = (labs(q) * 2 + 32640) / 65280;
    return (int)(q >= 0 ? m : -m);
}

static void lj_test_arith(void)
{
    lj_gm_param sym = lj_p(7, 1, 128, 186, 186, LJ_DIR_FWD, 0), pos = lj_p(7, 1, 128, 186, 0, LJ_DIR_FWD, 0),
                neg = lj_p(7, 1, 128, 0, 186, LJ_DIR_FWD, 0), mir = lj_p(7, 1, 128, -60, 0, LJ_DIR_FWD, 0);
    uint32_t incr = (uint32_t)(((uint64_t)8192 << 16) * 25000u / 1000000u), k;   /* cycle 1.0 s at 40 Hz */
    int lo = 0, hi = 0, n;
    /* the readout: Min/Max -186/186 for Pos/Neg 186 on 'Sine' [IMG genericmacroeditor.gif]: the unclamped delta over every index */
    for (k = 0; k < LJ_GM_SAMPLES; k++) {
        int d = lj_genmacro_delta(NULL, &sym, k << 16);
        lj_gm_param lo0 = sym, hi255 = sym;
        lo0.base = 0; hi255.base = 255;
        CHECK(lj_genmacro_step(NULL, d >= 0 ? &lo0 : &hi255, k << 16, 0, 0) == (d >= 0 ? d : 255 + d));   /* step = clamp(base + delta), each half where it cannot clamp */
        if (d < lo) lo = d;
        if (d > hi) hi = d;
        CHECK(d == lj_ref_delta(lj_genmacro_shape_in(NULL, 7, (int)k), 186, 186));
    }
    CHECK(lo == -LJ_GENMACRO_AMP186_EXAMPLE && hi == LJ_GENMACRO_AMP186_EXAMPLE);
    for (n = 0; n <= 40; n++) {                                /* tick by tick over one cycle, base 128, clamp 0..255 */
        uint32_t idx = (uint32_t)n * incr;
        int s = lj_genmacro_shape_in(NULL, 7, (int)(idx >> 16)), want = 128 + lj_ref_delta(s, 186, 186);
        want = want < 0 ? 0 : want > 255 ? 255 : want;
        CHECK(lj_genmacro_step(NULL, &sym, idx, 0, 0) == want);
        CHECK(lj_genmacro_step(NULL, &pos, idx, 0, 0) == (s >= 0 ? want : 128));   /* LORD_ENGINE-14: Amp+ alone moves the positive half only */
        CHECK(lj_genmacro_step(NULL, &neg, idx, 0, 0) == (s < 0 ? want : 128));    /* and Amp- the negative half only */
    }
    CHECK(lj_genmacro_step(NULL, &sym, 10 * incr, 0, 0) == 255 && lj_genmacro_step(NULL, &sym, 30 * incr, 0, 0) == 0);   /* 128 +- 186 clamped */
    CHECK(lj_genmacro_step(NULL, &sym, 2 * incr, 0, 0) == 185);                     /* index 409: Sine 10072 -> +57 */
    CHECK(lj_genmacro_step(NULL, &mir, 10 * incr, 0, 0) == 68);                     /* a negative Amp+ mirrors the hump below the base (CHM 5819) */
    sym.base_on = 0;
    CHECK(lj_genmacro_step(NULL, &sym, 0, 77, 0) == 77 && lj_genmacro_step(NULL, &sym, 10 * incr, 77, 0) == 255);   /* base off: ref = the merged value */
    {   /* delay subtracts from the index; bounce ignores it (CHM 5571, 5835) */
        lj_gm_param d = lj_p(7, 1, 128, 100, 100, LJ_DIR_FWD, 2048), bo = lj_p(7, 1, 128, 100, 100, LJ_DIR_BOUNCE, 2048);
        CHECK(lj_genmacro_step(NULL, &d, 4096u << 16, 0, 0) == 228 && lj_genmacro_step(NULL, &bo, 2048u << 16, 0, 0) == 228);
    }
}

static void lj_test_advance(void)
{
    lj_genmacro *m = lj_genmacro_new_rec();
    lj_gm_ch st[LJ_CHANS];
    int t;
    memset(st, 0, sizeof st);
    m->ch[0] = lj_p(7, 1, 128, 10, 10, LJ_DIR_FWD, 0);
    m->ch[1] = lj_p(7, 1, 128, 10, 10, LJ_DIR_REV, 0);
    m->ch[2] = lj_p(7, 1, 128, 10, 10, LJ_DIR_BOUNCE, 0);
    m->ch[3] = lj_p(7, 1, 128, 10, 10, LJ_DIR_FWD, 0); m->ch[3].cycle_ds = 0;   /* static */
    m->ch[4].shape = 0;                                                         /* not programmed: never advances */
    for (t = 0; t < 10; t++) lj_genmacro_advance(m, st, 25000);
    CHECK(st[0].idx >> 16 == 2047 && st[1].idx >> 16 == 8192 - 2048 && st[2].idx >> 16 == 2047 && st[3].idx == 0 && st[4].idx == 0);
    for (t = 10; t < 45; t++) lj_genmacro_advance(m, st, 25000);
    CHECK(st[0].idx >> 16 == (45u * 13421772u >> 16) - 8192 && st[2].back == 1 && st[2].idx >> 16 < 8192 && st[2].idx >> 16 > 6000);   /* bounced off 8191 */
    free(m);
}

/* ---- ME-34: synthetic Macros.Mac files (no Martin bytes) ---- */
static int lj_write_mac(const char *path, int n, int len_override, long cut)
{
    FILE *f = fopen(path, "wb");
    long size = 4104L + (long)n * 16415L, i;
    unsigned char *b = (unsigned char *)calloc(1, (size_t)size);
    int r, k;
    if (!f || !b) { if (f) fclose(f); free(b); return -1; }
    b[0] = 1; b[4] = (unsigned char)n;
    for (r = 0; r < n; r++) {
        unsigned char *rec = b + 4104 + (long)r * 16415;
        rec[0] = (unsigned char)(len_override >= 0 ? len_override : 4); memcpy(rec + 1, "Usr0", 4); rec[4] = (unsigned char)('0' + r);
        for (k = 0; k < 8192; k++) { int v = r * 1000 + k; rec[31 + 2 * k] = (unsigned char)(v & 0xFF); rec[32 + 2 * k] = (unsigned char)(v >> 8); }
    }
    i = cut >= 0 ? cut : size;
    r = fwrite(b, 1, (size_t)i, f) == (size_t)i ? 0 : -1;
    fclose(f); free(b);
    return r;
}

static void lj_test_override(void)
{
    lj_gm_shapes sh;
    char err[256];
    lj_gm_param p = lj_p(1, 1, 0, 255, 255, LJ_DIR_FWD, 0);
    long cut;
    CHECK(lj_write_mac("test_genmacro_Macros.Mac", 2, -1, -1) == 0 && lj_write_mac("test_genmacro_UserMacros.Mac", 1, -1, -1) == 0);
    CHECK(lj_gm_shapes_load("test_genmacro_Macros.Mac", "test_genmacro_UserMacros.Mac", &sh, err, sizeof err) == 0);
    CHECK(sh.n == 3 && sh.nb == 2 && !strcmp(sh.name[0], "Usr0") && !strcmp(sh.name[1], "Usr1") && !strcmp(sh.name[2], "Usr0"));
    CHECK(lj_genmacro_shape_in(&sh, 1, 5) == 1005 && lj_genmacro_shape_in(&sh, 2, 7) == 7 && lj_genmacro_shape_in(&sh, 3, 7) == 0);   /* its values, not the built-ins */
    CHECK(lj_genmacro_step(&sh, &p, 5u << 16, 0, 0) == (1005 * 255 + 16320) / 32640);
    lj_gm_shapes_free(&sh);
    CHECK(sh.s == NULL && sh.n == 0);
    CHECK(lj_gm_shapes_load("test_genmacro_Macros.Mac", "test_genmacro_none.Mac", &sh, err, sizeof err) == 0 && sh.n == 2);   /* no UserMacros.Mac: no user shapes */
    lj_gm_shapes_free(&sh);
    CHECK(lj_gm_shapes_load(NULL, NULL, &sh, err, sizeof err) == -1 && sh.s == NULL);   /* no override: the app keeps the built-ins */
    CHECK(lj_genmacro_shape_in(NULL, 7, 2048) == 32640);
    CHECK(lj_write_mac("test_genmacro_bad.Mac", 1, 16, -1) == 0 && lj_gm_shapes_load("test_genmacro_bad.Mac", NULL, &sh, err, sizeof err) == -1 && strstr(err, "name"));
    for (cut = 0; cut < 4104L + 16415L; cut += cut < 16 ? 1 : 61) {             /* every truncated prefix is refused (rule 6) */
        if (lj_write_mac("test_genmacro_bad.Mac", 1, -1, cut)) { fails++; break; }
        if (lj_gm_shapes_load("test_genmacro_bad.Mac", NULL, &sh, err, sizeof err) != -1) { fails++; printf("FAIL truncated Macros.Mac of %ld bytes accepted\n", cut); lj_gm_shapes_free(&sh); }
    }
    remove("test_genmacro_Macros.Mac"); remove("test_genmacro_UserMacros.Mac"); remove("test_genmacro_bad.Mac");
}

static void lj_write_text(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

static void lj_test_cma(void)
{
    lj_genmacro *m = lj_genmacro_new_rec(), *m2 = lj_genmacro_new_rec();
    char err[256];
    static const char *const bad[] = {   /* one field out of range per line: the whole file is refused */
        "shape=300 bon=1 base=1 amp=1,1 cycle=10 dir=fwd delay=0", "shape=7 bon=1 base=1 amp=400,0 cycle=10 dir=fwd delay=0",
        "shape=7 bon=1 base=1 amp=1 cycle=10 dir=fwd delay=0", "shape=7 bon=1 base=1 amp=1,1 cycle=10 dir=fwd delay=9000",
        "shape=7 bon=1 base=1 amp=1,1 cycle=6000 dir=fwd delay=0", "shape=7 bon=2 base=1 amp=1,1 cycle=10 dir=fwd delay=0",
        "shape=7 bon=1 base=256 amp=1,1 cycle=10 dir=fwd delay=0", "shape=7 bon=1 base=1 cycle=10 dir=fwd delay=0" };
    int i, c = lj_addr_code(2, 9);
    m->number = 12; strcpy(m->name, "Two sine"); m->restart_on_load = 1;
    m->ch[c] = lj_p(0, 0, 0, 45, -45, LJ_DIR_BOUNCE, 8191);       /* shape 0 '2*Sine' with bounce: W1 dropped both */
    m->ch[c + 1] = lj_p(65, 1, 0, 0, 0, LJ_DIR_REV, 0);
    CHECK(lj_genmacro_validate(m) == NULL);
    CHECK(lj_genmacro_save("test_genmacro.CMA", m) == 0 && lj_genmacro_load("test_genmacro.CMA", m2, err, sizeof err) == 0 && memcmp(m, m2, sizeof *m) == 0);
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        char text[200];
        snprintf(text, sizeof text, "LJ4 macro 1\nnum=3\nname=x\nrestart=0\nch 1:1 %s\n", bad[i]);
        lj_write_text("test_genmacro.CMA", text);
        if (lj_genmacro_load("test_genmacro.CMA", m2, err, sizeof err) != -1) { fails++; printf("FAIL .CMA with %s accepted\n", bad[i]); }
    }
    lj_write_text("test_genmacro.CMA", "LJ4 macro 1\nnum=10000\n");
    CHECK(lj_genmacro_load("test_genmacro.CMA", m2, err, sizeof err) == -1);
    m->ch[c].dir = 4;
    CHECK(lj_genmacro_validate(m) != NULL);
    remove("test_genmacro.CMA");
    free(m); free(m2);
}

static void lj_test_auto_delay(void)
{
    uint16_t d[16];
    uint32_t seed = 7;
    int i;
    CHECK(lj_genmacro_auto_delay(10, LJ_GMD_LINEAR, 0, 8191, 1, 0, NULL, d) == 0);   /* 10 fixtures: 0 %, 10 %, ... 90 % */
    for (i = 0; i < 10; i++) CHECK(d[i] == 8192 * i / 10);
    CHECK(lj_genmacro_auto_delay(12, LJ_GMD_CIRCULAR, 0, 8191, 1, 3, NULL, d) == 0);   /* 12 fixtures, 3 groups: 0/25/50/75 % repeated */
    for (i = 0; i < 12; i++) CHECK(d[i] == 2048 * (i % 4));
    CHECK(lj_genmacro_auto_delay(12, LJ_GMD_SEGMENTS, 0, 8191, 1, 4, NULL, d) == 0);   /* 12 fixtures, 4 groups: 0, 25, 50, 75 % */
    for (i = 0; i < 12; i++) CHECK(d[i] == 2048 * (i / 3));
    CHECK(lj_genmacro_auto_delay(3, LJ_GMD_LINEAR, 6000, 2000, 0, 0, NULL, d) == 0 && d[0] == 6000 && d[1] == 8096 && d[2] == 2000);   /* To < From: 6000 -> 8191 -> 0 -> 2000 */
    CHECK(lj_genmacro_auto_delay(16, LJ_GMD_RANDOM, 100, 200, 0, 0, &seed, d) == 0);
    for (i = 0; i < 16; i++) CHECK(d[i] >= 100 && d[i] <= 200);
    CHECK(lj_genmacro_auto_delay(0, LJ_GMD_LINEAR, 0, 0, 1, 0, NULL, d) == -1 && lj_genmacro_auto_delay(4, LJ_GMD_CIRCULAR, 0, 0, 1, 1, NULL, d) == -1);
}

int lj_test_genmacro(int argc, char **argv);
int lj_test_genmacro(int argc, char **argv)
{
    (void)argc; (void)argv;
    lj_test_shapes();
    lj_test_arith();
    lj_test_advance();
    lj_test_override();
    lj_test_cma();
    lj_test_auto_delay();
    printf("test_genmacro: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
