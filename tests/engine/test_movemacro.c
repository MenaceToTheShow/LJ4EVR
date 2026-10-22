/* test_movemacro.c - `lj4evr_tests test_movemacro` (W3 movement, design 10 W3 acceptance 4):
 *  - every lj_vec_mm_trace row (17-position_ref.py section 7 through tests/vectors.h) equals lj_movemacro_pos;
 *  - Fan Out offsets / lj_fan_out / Spread (17 s5) and Auto Delay / delay order (17 s6) equal their vectors;
 *  - Bezier: equal points per segment (04 s4.5), nodes on the segment starts, +-32767 span, .bez round trip, the
 *    loader refusing out-of-range values, and a Bezier type through lj_movemacro_pos / lj_movemacro_offset_bez;
 *  - the movement macro's 16-bit pan (ME-22) lands in the runner as one value (replay movemacro_six_shapes holds the
 *    engine-level expects, ME-35 one step per tick). */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_movemacro.h"
#include "model/lj_genmacro.h"
#include "model/lj_movemacro.h"
#include "vectors.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void traces(void)
{
    int i, bad = 0;
    for (i = 0; i < COUNT(lj_vec_mm_trace); i++) {
        lj_movemacro m; lj_mm_state s; int32_t p, t;
        memset(&m, 0, sizeof m);
        m.type = lj_vec_mm_trace[i].type; m.amp_pan = lj_vec_mm_trace[i].amp_pan; m.amp_tilt = lj_vec_mm_trace[i].amp_tilt;
        m.delay = lj_vec_mm_trace[i].delay; m.tweak = lj_vec_mm_trace[i].tweak; m.step = lj_vec_mm_trace[i].step; m.fstate[LJ_MMP_TYPE] = LJ_SNAP;
        s.idx_pan = (uint32_t)lj_vec_mm_trace[i].ip << 16; s.idx_tilt = (uint32_t)lj_vec_mm_trace[i].it << 16;
        CHECK(lj_movemacro_pos(&s, &m, NULL, lj_vec_mm_trace[i].pct, lj_vec_mm_trace[i].base_pan, lj_vec_mm_trace[i].base_tilt, &p, &t) == 0);
        if (p != lj_vec_mm_trace[i].pan || t != lj_vec_mm_trace[i].tilt) {
            if (bad++ < 10) printf("FAIL trace %d: type %d ip %d it %d amp %d/%d pct %d delay %d tweak %d step %d base %d:%d -> %d:%d, oracle %d:%d\n", i,
                                   m.type, lj_vec_mm_trace[i].ip, lj_vec_mm_trace[i].it, m.amp_pan, m.amp_tilt, lj_vec_mm_trace[i].pct, m.delay, m.tweak, m.step,
                                   (int)lj_vec_mm_trace[i].base_pan, (int)lj_vec_mm_trace[i].base_tilt, (int)p, (int)t, (int)lj_vec_mm_trace[i].pan, (int)lj_vec_mm_trace[i].tilt);
        }
    }
    fails += bad;
    printf("test_movemacro: %d trace(s) vs 17-position_ref.py, %d mismatch(es)\n", COUNT(lj_vec_mm_trace), bad);
    {   /* inactive: No Macro, fade state Off, a Bezier type without shapes -> base, -1 */
        lj_movemacro m; lj_mm_state s = { 0, 0 }; int32_t p, t;
        memset(&m, 0, sizeof m);
        CHECK(lj_movemacro_pos(&s, &m, NULL, 100, 5, 6, &p, &t) == -1 && p == 5 && t == 6);
        m.type = LJ_MM_TYPE_ELLIPSE; m.amp_pan = 127; m.fstate[LJ_MMP_TYPE] = LJ_OFF;
        CHECK(lj_movemacro_pos(&s, &m, NULL, 100, 5, 6, &p, &t) == -1);
        m.type = LJ_MM_TYPE_BEZ0; m.fstate[LJ_MMP_TYPE] = LJ_SNAP;
        CHECK(lj_movemacro_pos(&s, &m, NULL, 100, 5, 6, &p, &t) == -1 && p == 5);
    }
}

static void fan_and_delay(void)
{
    int i, k;
    for (i = 0; i < COUNT(lj_vec_fanout); i++) {
        int32_t o[8];
        lj_fan_out_offsets(lj_vec_fanout[i].n, lj_vec_fanout[i].delta, o);
        for (k = 0; k < lj_vec_fanout[i].n; k++) CHECK(o[k] == lj_vec_fanout[i].off[k]);
    }
    for (i = 0; i < COUNT(lj_vec_fan); i++) {
        int32_t pan[6], tilt[6];
        memcpy(pan, lj_vec_fan[i].in_pan, sizeof pan); memcpy(tilt, lj_vec_fan[i].in_tilt, sizeof tilt);
        CHECK(lj_fan_out(pan, tilt, lj_vec_fan[i].order, lj_vec_fan[i].n, lj_vec_fan[i].dpan, lj_vec_fan[i].dtilt, lj_vec_fan[i].home) == 0);
        for (k = 0; k < lj_vec_fan[i].n; k++) CHECK(pan[k] == lj_vec_fan[i].pan[k] && tilt[k] == lj_vec_fan[i].tilt[k]);
    }
    { int32_t p[2] = { 0, 0 }, t[2] = { 0, 0 }; int o[1] = { 0 }; CHECK(lj_fan_out(p, t, o, 1, 10, 10, 1) == -1); }   /* "2 or more" [04 s4.2] */
    for (i = 0; i < COUNT(lj_vec_spread); i++) {
        int32_t v[5];
        memcpy(v, lj_vec_spread[i].in, sizeof v);
        CHECK(lj_fan_spread(v, lj_vec_spread[i].order, lj_vec_spread[i].n) == 0);
        for (k = 0; k < lj_vec_spread[i].n; k++) CHECK(v[k] == lj_vec_spread[i].out[k]);
    }
    for (i = 0; i < COUNT(lj_vec_autodelay); i++) {
        uint8_t d[8];
        int st = !strcmp(lj_vec_autodelay[i].style, "linear") ? LJ_MMD_LINEAR : !strcmp(lj_vec_autodelay[i].style, "circular") ? LJ_MMD_CIRCULAR : LJ_MMD_SEGMENTS;
        CHECK(lj_mm_auto_delay(lj_vec_autodelay[i].n, st, lj_vec_autodelay[i].max_delay, lj_vec_autodelay[i].param, lj_vec_autodelay[i].param, NULL, d) == 0);
        for (k = 0; k < lj_vec_autodelay[i].n; k++) CHECK(d[k] == lj_vec_autodelay[i].d[k]);
    }
    {   /* Random: inside 0..max-1, deterministic from the seed; bad arguments refused */
        uint8_t a[50], b[50]; uint32_t s1 = 7, s2 = 7;
        CHECK(lj_mm_auto_delay(50, LJ_MMD_RANDOM, 64, 0, 0, &s1, a) == 0 && lj_mm_auto_delay(50, LJ_MMD_RANDOM, 64, 0, 0, &s2, b) == 0);
        CHECK(memcmp(a, b, sizeof a) == 0 && s1 == s2 && s1 != 7);
        for (k = 0; k < 50; k++) CHECK(a[k] < 64);
        CHECK(lj_mm_auto_delay(0, LJ_MMD_LINEAR, 256, 0, 0, NULL, a) == -1 && lj_mm_auto_delay(4, LJ_MMD_LINEAR, 257, 0, 0, NULL, a) == -1);
        CHECK(lj_mm_auto_delay(4, LJ_MMD_CIRCULAR, 256, 0, 0, NULL, a) == -1 && lj_mm_auto_delay(4, LJ_MMD_SEGMENTS, 256, 0, 0, NULL, a) == -1);
    }
    {
        lj_show *show = (lj_show *)calloc(1, sizeof *show);
        if (!show) { fails++; return; }
        for (i = 0; i < COUNT(lj_vec_delay_order); i++) {
            int o[5];
            for (k = 0; k < 5; k++) { show->fixture[k].icon_x = lj_vec_delay_order[i].ix[k]; show->fixture[k].icon_y = lj_vec_delay_order[i].iy[k]; }
            CHECK(lj_delay_order(show, lj_vec_delay_order[i].sel, 5, lj_vec_delay_order[i].mode, o) == 0);
            for (k = 0; k < 5; k++) CHECK(o[k] == lj_vec_delay_order[i].order[k]);
        }
        free(show);
    }
}

static void bezier(void)
{
    lj_bez b, r; lj_bez_table *t = (lj_bez_table *)malloc(sizeof *t), *t2 = (lj_bez_table *)malloc(sizeof *t2);
    lj_bez_set set; lj_movemacro m; lj_mm_state s = { 0, 0 }; int32_t dp, dt, p, q;
    char err[256]; FILE *f;
    int k;
    if (!t || !t2) { fails++; free(t); free(t2); return; }
    memset(&b, 0, sizeof b);
    strcpy(b.name, "Diamond");
    {   /* the beziershapeeditor.gif diamond: four nodes, straight segments */
        static const int16_t xy[4][2] = { { 0, -20000 }, { 10000, 0 }, { 0, 20000 }, { -10000, 0 } };
        for (k = 0; k < 4; k++) { lj_bez_node *n = &b.node[k]; n->x = n->in_x = n->out_x = xy[k][0]; n->y = n->in_y = n->out_y = xy[k][1]; n->line = 1; }
        b.n = 4;
    }
    CHECK(lj_bez_validate(&b) == NULL && lj_bez_sample(&b, t) == 0);
    CHECK(t->pan[0] == 0 && t->tilt[0] == -32767 && t->pan[1024] == 32767 && t->tilt[1024] == 0 && t->pan[2048] == 0 && t->tilt[2048] == 32767 && t->pan[3072] == -32767);   /* a node starts every 1024 points: equal time per segment */
    CHECK(t->pan[512] == 16384 && t->tilt[512] == -16384);   /* the middle of the first straight segment (rounded half away from zero) */
    for (k = 1; k < 1024; k++) CHECK(t->pan[k] > t->pan[k - 1]);
    for (k = 0; k < 4; k++) b.node[k].line = 0;              /* handles on the nodes: the cubic degenerates to a line with eased spacing */
    CHECK(lj_bez_sample(&b, t2) == 0 && t2->pan[1024] == 32767 && t2->pan[0] == 0);
    b.n = 3; CHECK(lj_bez_sample(&b, t2) == 0);              /* 4096 / 3: 1365, 1365, 1366 (remainder on the last segment) */
    CHECK(t2->tilt[0] == -32767 && t2->pan[1365] == 32767 && t2->tilt[2730] == 32767);
    b.n = 4;
    b.n = 2; CHECK(lj_bez_validate(&b) != NULL && lj_bez_sample(&b, t2) == -1); b.n = 4;
    b.node[1].out_x = 15000; b.node[2].in_y = 25000; b.node[3].line = 1;   /* round trip keeps handles and line flags */
    CHECK(lj_bez_save("test_movemacro_tmp.bez", &b) == 0);
    CHECK(lj_bez_load("test_movemacro_tmp.bez", &r, err, sizeof err) == 0 && r.n == 4 && !strcmp(r.name, "Diamond"));
    CHECK(memcmp(r.node, b.node, sizeof b.node[0] * 4) == 0);
    if ((f = fopen("test_movemacro_tmp.bez", "wb")) != NULL) { fputs("LJ4 bez 1\nname=Bad\nnode x=40000 y=0\nnode x=0 y=0\nnode x=1 y=1\n", f); fclose(f); }
    CHECK(lj_bez_load("test_movemacro_tmp.bez", &r, err, sizeof err) == -1 && strstr(err, "out of range") != NULL);
    if ((f = fopen("test_movemacro_tmp.bez", "wb")) != NULL) { fputs("LJ4 bez 1\nname=Two\nnode x=1 y=0\nnode x=0 y=0\n", f); fclose(f); }
    CHECK(lj_bez_load("test_movemacro_tmp.bez", &r, err, sizeof err) == -1);
    remove("test_movemacro_tmp.bez");
    /* a Bezier macro is "just two value arrays like a system macro" [04 s4.5]: index 1024, amplitude 127 = half range */
    set.n = 1; set.t = t;
    memset(&m, 0, sizeof m);
    m.type = LJ_MM_TYPE_BEZ0; m.amp_pan = m.amp_tilt = 127; m.fstate[LJ_MMP_TYPE] = LJ_SNAP;
    s.idx_pan = s.idx_tilt = 1024u << 16;
    lj_movemacro_offset_bez(&s, &m, &set, 100, &dp, &dt);
    CHECK(dp == 32768 && dt == 0);
    CHECK(lj_movemacro_pos(&s, &m, &set, 100, 0, 0, &p, &q) == 0 && p == 32767 && q == 0);   /* clamped at the edge */
    CHECK(lj_movemacro_pos(&s, &m, &set, 45, -32768, 0, &p, &q) == 0 && p == -32768 + 14707);   /* cue macro control 45 %: 127 -> 57, round(57 * 32768 / 127) */
    m.type = LJ_MM_TYPE_BEZ0 + 1;
    lj_movemacro_offset_bez(&s, &m, &set, 100, &dp, &dt);
    CHECK(dp == 0 && dt == 0);                               /* shape 2 is not loaded */
    {   /* the engine advances a Bezier type on one common cycle (the figure keeps its shape) */
        lj_movemacro_advance(&s, &m, 25000, 100);
        m.type = LJ_MM_TYPE_BEZ0; m.cycle_pan = 10; m.cycle_tilt = 40; s.idx_pan = s.idx_tilt = 0;
        lj_movemacro_advance(&s, &m, 25000, 100);
        CHECK(s.idx_pan == s.idx_tilt && s.idx_pan == (uint32_t)((4096ull << 16) * 25000 / 1000000));
    }
    free(t); free(t2);
}

int lj_test_movemacro(int argc, char **argv);
int lj_test_movemacro(int argc, char **argv)
{
    (void)argc; (void)argv;
    traces();
    fan_and_delay();
    bezier();
    printf("test_movemacro: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
