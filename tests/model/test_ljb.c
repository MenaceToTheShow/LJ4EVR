/* test_ljb.c - the ljb track's rows (docs/w5-contracts.md seam 8). The contract seeds the fuzz row (W5-X-HOSTILE):
 *   fuzz_ljb  a synthetic .LJB built here (note 18 s1: two zlib entries and a stored one, a zlib index of 532-byte
 *             records, the 36-byte trailer with its CRC) must open and extract byte-exact, then LJ_FUZZ_ITERS mutations
 *             through lj_ljb_open / lj_ljb_extract; the trailer CRC is recomputed on every second one so the walk
 *             behind the CRC check is fuzzed too
 * It prints PENDING ljb and fails until the track lands the reader (DISABLED in CMakeLists.txt until then). ljb_build is
 * the synthetic seed every ljb row may reuse (tests never ship a Martin file). The track appends W5-LJB-1..5 rows. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_ljb.h"
#include "miniz.h"
#include "lj_fuzz.h"

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static size_t ljb_build(uint8_t *out, size_t cap)   /* the container; 0 when cap is too small */
{
    static const struct { const char *name, *folder, *data; int stored; } ent[3] = {
        { "Config.dat", "", "synthetic patch record, not LJ2 data", 0 },
        { "00000001.seq", "Sequences\\", "synthetic sequence bytes 0123456789", 0 },
        { "DirInfo.Ini", "", "[Info]\r\nName=Fuzz\r\n", 1 } };
    static uint8_t idx[3 * LJ_LJB_INDEX_REC];
    size_t len = 0, k, usz;
    mz_ulong zl;
    memset(idx, 0, sizeof idx);
    for (k = 0; k < 3; k++) {
        uint8_t *r = idx + k * LJ_LJB_INDEX_REC;
        usz = strlen(ent[k].data);
        r[0] = (uint8_t)strlen(ent[k].name); memcpy(r + 1, ent[k].name, r[0]);
        r[256] = (uint8_t)strlen(ent[k].folder); memcpy(r + 257, ent[k].folder, r[256]);
        zl = (mz_ulong)(cap - len);
        if (ent[k].stored) { if (usz > cap - len) return 0; memcpy(out + len, ent[k].data, usz); zl = (mz_ulong)usz; }
        else if (mz_compress2(out + len, &zl, (const unsigned char *)ent[k].data, (mz_ulong)usz, 9) != MZ_OK) return 0;
        le32(r + 512, (uint32_t)usz); le32(r + 516, (uint32_t)zl); le32(r + 520, (uint32_t)len);
        le32(r + 524, (uint32_t)mz_crc32(MZ_CRC32_INIT, (const unsigned char *)ent[k].data, usz));
        le32(r + 528, ent[k].stored ? LJ_LJB_FLAG_STORED : LJ_LJB_FLAG_DEFLATE);
        len += zl;
    }
    zl = (mz_ulong)(cap - len - LJ_LJB_TRAILER);
    if (mz_compress2(out + len, &zl, idx, sizeof idx, 9) != MZ_OK) return 0;
    memset(out + len + zl, 0, LJ_LJB_TRAILER);
    le32(out + len + zl + 4, 3); le32(out + len + zl + 8, 1); le32(out + len + zl + 12, (uint32_t)zl); le32(out + len + zl + 16, (uint32_t)len); le32(out + len + zl + 20, 1);
    len += zl;
    le32(out + len, (uint32_t)mz_crc32(MZ_CRC32_INIT, out, len));
    return len + LJ_LJB_TRAILER;
}

int lj_test_fuzz_ljb(int argc, char **argv)
{
    static uint8_t seed[8192], buf[8192 + 64];
    size_t sn = ljb_build(seed, sizeof seed), n, xn = 0;
    uint8_t *x = NULL;
    lj_ljb b;
    char err[128];
    uint32_t s = 0x1B7u, k;
    int i, bad = 0;
    (void)argc; (void)argv;
    if (!sn) { printf("FAIL ljb_build\n"); return 1; }
    if (lj_ljb_open(&b, seed, sn, err, sizeof err) != 0 || b.n != 3 || strcmp(b.e[1].folder, "Sequences\\") != 0
        || lj_ljb_extract(&b, 2, &x, &xn, err, sizeof err) != 0 || xn != 19 || memcmp(x, "[Info]\r\n", 8) != 0) {
        printf("PENDING ljb: lj_ljb_open / lj_ljb_extract do not read the synthetic container yet\n");
        free(x);
        return 1;
    }
    free(x); x = NULL; lj_ljb_free(&b);
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, seed, sn); n = sn;
        for (k = lj_fuzz_next(&s) % 8 + 1; k > 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        if ((i & 1) && n > LJ_LJB_TRAILER) le32(buf + n - LJ_LJB_TRAILER, (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, n - LJ_LJB_TRAILER));
        if (lj_ljb_open(&b, buf, n, err, sizeof err) != 0) continue;
        for (k = 0; k < b.n; k++) {
            if (lj_ljb_extract(&b, k, &x, &xn, err, sizeof err) == 0 && xn != b.e[k].usz) { if (!bad++) printf("FAIL iteration %d: entry %u size\n", i, (unsigned)k); }
            free(x); x = NULL;
        }
        lj_ljb_free(&b);
    }
    printf("fuzz_ljb: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
