/* test_trig.c - the committed trig tables (design 3.6): sin[1024] == 32767,
 * quarter-wave symmetry, cos == sin shifted, monotonic first quadrant, and the
 * generator's spot values from tests/vectors.h (which must compile and be
 * self-consistent: the .GS2 layout tiles the record exactly). */
#include <stdio.h>
#include <string.h>
#include "engine/lj_trig_gen.h"
#include "engine/lj_engine_rules.h"
#include "vectors.h"

_Static_assert(LJ_MOVEMACRO_N_TABLE == 4096, "lj_sin4096 size");
_Static_assert(sizeof lj_vec_sacn_data_b1 == 638 && sizeof lj_vec_sacn_sync_b2 == 49 && sizeof lj_vec_sacn_disc == 126, "E1.31 Appendix B sizes");

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_trig(int argc, char **argv);
int lj_test_trig(int argc, char **argv)
{
    int i, bad = 0, end = 0;
    (void)argc; (void)argv;
    CHECK(lj_sin4096[0] == 0 && lj_sin4096[1024] == 32767 && lj_sin4096[2048] == 0 && lj_sin4096[3072] == -32767);
    CHECK(lj_cos4096[0] == 32767 && lj_cos4096[1024] == 0 && lj_cos4096[2048] == -32767);
    for (i = 0; i < 4096; i++) {
        if (lj_sin4096[i] != -lj_sin4096[(i + 2048) & 4095]) bad++;          /* half-wave antisymmetry */
        if (lj_sin4096[i] != lj_sin4096[(2048 - i) & 4095]) bad++;           /* mirror about 1024 */
        if (lj_cos4096[i] != lj_sin4096[(i + 1024) & 4095]) bad++;           /* cos = sin shifted */
        if (i > 0 && i <= 1024 && lj_sin4096[i] < lj_sin4096[i - 1]) bad++;  /* rising first quadrant */
    }
    CHECK(bad == 0);
    for (i = 0; i < (int)(sizeof lj_vec_trig / sizeof lj_vec_trig[0]); i++)
        if (lj_sin4096[lj_vec_trig[i].i] != lj_vec_trig[i].s || lj_cos4096[lj_vec_trig[i].i] != lj_vec_trig[i].c) {
            fails++; printf("FAIL vector i=%d\n", lj_vec_trig[i].i);
        }
    /* vectors.h self-consistency */
    CHECK(lj_vec_sacn_data_b1[16] == 0x72 && lj_vec_sacn_data_b1[17] == 0x6e && lj_vec_sacn_data_b1[111] == 154);
    CHECK(memcmp(lj_vec_sacn_data_b1 + 22, lj_vec_sacn_cid, 16) == 0);
    CHECK(strcmp(lj_vec_sacn_mcast[0].addr, "239.255.0.1") == 0);
    for (i = 0; i < (int)(sizeof lj_vec_gs2_layout / sizeof lj_vec_gs2_layout[0]); i++) {
        const char *k = lj_vec_gs2_layout[i].kind;
        int n = lj_vec_gs2_layout[i].n, sz;
        if (lj_vec_gs2_layout[i].off < end) { fails++; printf("FAIL gs2 layout overlap at %s\n", lj_vec_gs2_layout[i].name); }
        else if (lj_vec_gs2_layout[i].off > end)
            printf("test_trig: gs2 layout leaves %d unlisted byte(s) before %s (ref 16 LAYOUT)\n", lj_vec_gs2_layout[i].off - end, lj_vec_gs2_layout[i].name);
        sz = !strcmp(k, "dyn") ? LJ_VEC_GS2_DYN_SIZE : !strcmp(k, "dyns") ? n * LJ_VEC_GS2_DYN_SIZE
           : !strcmp(k, "static") ? LJ_VEC_GS2_STATIC_SIZE : !strcmp(k, "statics") ? n * LJ_VEC_GS2_STATIC_SIZE
           : !strcmp(k, "pals") ? n * LJ_VEC_GS2_PAL_SIZE : !strcmp(k, "levels") ? n * LJ_VEC_GS2_LEVEL_SIZE
           : !strcmp(k, "s") ? n + 1 : n;
        end = lj_vec_gs2_layout[i].off + sz;
    }
    CHECK(end == LJ_VEC_GS2_SIZE);
    CHECK(lj_vec_pos16[0].readout == -32768 && lj_vec_pos16[0].dmx16 == 0 && lj_vec_scale_raw[0].out == 57);
    printf("test_trig: %d symmetry violation(s), %d failure(s)\n", bad, fails);
    return fails ? 1 : 0;
}
