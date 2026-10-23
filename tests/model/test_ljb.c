/* test_ljb.c - the ljb track's rows (docs/w5-contracts.md seam 8; W5-LJB-1..5, ME-8, LORD_C-9). Every .LJB here is
 * SYNTHETIC, written by ljb_pack below (note 18 s1: entries zlib-deflated or stored, a zlib index of 532-byte records,
 * the 36-byte trailer with its CRC); no LJ2 file is ever read in CI or committed.
 *   fuzz_ljb    W5-X-HOSTILE: the three-entry seed opens and extracts byte-exact, then LJ_FUZZ_ITERS mutations through
 *               lj_ljb_open / lj_ljb_extract; the trailer CRC is recomputed on every second one so the walk behind
 *               the CRC check is fuzzed too
 *   test_ljb    W5-LJB-1 one refusal per container rule; W5-LJB-2 hostile names (path_ok rows and a traversal file
 *               imported under a scratch root: refused, nothing written, no library left); W5-LJB-3 a synthetic
 *               library of every converted kind read back through the LJ4 loaders (ME-8 ids kept); W5-LJB-4 the
 *               report and LJBImport.txt, Info.$$$ sanitised, Notes.$$$ stripped, the Restore window entry headless
 *               (an .LJB listed, picked, imported, reported, the new library selected); LORD_C-9 the notes.txt cap
 *   ljb_sample  W5-LJB-5, developer PC only: the .LJB under LJ4EVR_REFERENCE_DIR/notes/ledtrix opens (127 entries, every
 *               size and CRC), and the import counts agree with note 18; prints counts only, SKIP without the file */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "fixlib/lj_gs2.h"
#include "model/lj_cue.h"
#include "model/lj_cuepage.h"
#include "model/lj_genmacro.h"
#include "model/lj_ljb.h"
#include "model/lj_notes.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "desktop/lj_desktop.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_backup_restore.h"
#include "windows/lj_user_libraries.h"
#include "windows/lj_windows.h"
#include "windows/lj_warnings.h"
#include "miniz.h"
#include "lj_fuzz.h"

#ifndef LJ4EVR_REFERENCE_DIR
#define LJ4EVR_REFERENCE_DIR ""
#endif
#define TMP "test_ljb_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void le16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void sstr_put(uint8_t *p, const char *s) { p[0] = (uint8_t)strlen(s); memcpy(p + 1, s, p[0]); }

/* one entry of a synthetic container; namelen > 0 overrides the ShortString length (a NUL inside a name), the
 * overrides 0 = the honest value */
typedef struct { const char *name, *folder; const uint8_t *data; size_t len; int stored; uint32_t flags, usz, off, csz; int namelen; } ljb_ent;

static size_t ljb_pack(uint8_t *out, size_t cap, const ljb_ent *ent, int n)   /* the container; 0 when cap is too small */
{
    uint8_t *idx = (uint8_t *)calloc((size_t)n, LJ_LJB_INDEX_REC);
    size_t len = 0;
    mz_ulong zl;
    int k;
    if (!idx) return 0;
    for (k = 0; k < n; k++) {
        uint8_t *r = idx + (size_t)k * LJ_LJB_INDEX_REC;
        const ljb_ent *e = &ent[k];
        r[0] = (uint8_t)(e->namelen ? e->namelen : (int)strlen(e->name)); memcpy(r + 1, e->name, strlen(e->name) + 1 > 255 ? 255 : strlen(e->name) + 1);
        r[256] = (uint8_t)strlen(e->folder); memcpy(r + 257, e->folder, r[256]);
        zl = (mz_ulong)(cap - len);
        if (e->stored) { if (e->len > cap - len) { free(idx); return 0; } memcpy(out + len, e->data, e->len); zl = (mz_ulong)e->len; }
        else if (mz_compress2(out + len, &zl, e->data, (mz_ulong)e->len, 9) != MZ_OK) { free(idx); return 0; }
        le32(r + 512, e->usz ? e->usz : (uint32_t)e->len); le32(r + 516, e->csz ? e->csz : (uint32_t)zl); le32(r + 520, e->off ? e->off : (uint32_t)len);
        le32(r + 524, (uint32_t)mz_crc32(MZ_CRC32_INIT, e->data, e->len));
        le32(r + 528, e->flags ? e->flags : e->stored ? LJ_LJB_FLAG_STORED : LJ_LJB_FLAG_DEFLATE);
        len += zl;
    }
    zl = (mz_ulong)(cap - len - LJ_LJB_TRAILER);
    if (mz_compress2(out + len, &zl, idx, (mz_ulong)n * LJ_LJB_INDEX_REC, 9) != MZ_OK) { free(idx); return 0; }
    free(idx);
    memset(out + len + zl, 0, LJ_LJB_TRAILER);
    le32(out + len + zl + 4, (uint32_t)n); le32(out + len + zl + 8, 1); le32(out + len + zl + 12, (uint32_t)zl); le32(out + len + zl + 16, (uint32_t)len); le32(out + len + zl + 20, 1);
    len += zl;
    le32(out + len, (uint32_t)mz_crc32(MZ_CRC32_INIT, out, len));
    return len + LJ_LJB_TRAILER;
}
static void recrc(uint8_t *b, size_t n) { le32(b + n - LJ_LJB_TRAILER, (uint32_t)mz_crc32(MZ_CRC32_INIT, b, n - LJ_LJB_TRAILER)); }

static size_t ljb_build(uint8_t *out, size_t cap)   /* the fuzz seed: two zlib entries and a stored one */
{
    static const char *const d[3] = { "synthetic patch record, not LJ2 data", "synthetic sequence bytes 0123456789", "[Info]\r\nName=Fuzz\r\n" };
    ljb_ent e[3];
    memset(e, 0, sizeof e);
    e[0].name = "Config.dat";   e[0].folder = "";            e[0].data = (const uint8_t *)d[0]; e[0].len = strlen(d[0]);
    e[1].name = "00000001.seq"; e[1].folder = "Sequences\\"; e[1].data = (const uint8_t *)d[1]; e[1].len = strlen(d[1]);
    e[2].name = "DirInfo.Ini";  e[2].folder = "";            e[2].data = (const uint8_t *)d[2]; e[2].len = strlen(d[2]); e[2].stored = 1;
    return ljb_pack(out, cap, e, 3);
}

static uint32_t rd32le(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* LORD_C-7: the container c (cn bytes, trailer honest) with its index replaced by the raw records idx[0..in), deflated
 * again when z, else stored raw (the fuzz_ljb_index libFuzzer seed); trailer size and CRC rewritten. 0 when it does not fit. */
static size_t ljb_reindex(uint8_t *out, size_t cap, const uint8_t *c, size_t cn, const uint8_t *idx, size_t in, int z)
{
    uint32_t ioff = rd32le(c + cn - LJ_LJB_TRAILER + 16);
    mz_ulong zl;
    if (ioff > cap || cap - ioff < LJ_LJB_TRAILER) return 0;
    memcpy(out, c, ioff);
    zl = (mz_ulong)(cap - ioff - LJ_LJB_TRAILER);
    if (z ? mz_compress2(out + ioff, &zl, idx, (mz_ulong)in, 6) != MZ_OK : in > zl) return 0;
    if (!z) { memcpy(out + ioff, idx, in); zl = (mz_ulong)in; }
    memcpy(out + ioff + zl, c + cn - LJ_LJB_TRAILER, LJ_LJB_TRAILER);
    le32(out + ioff + zl + 12, (uint32_t)zl);
    recrc(out, ioff + zl + LJ_LJB_TRAILER);
    return ioff + zl + LJ_LJB_TRAILER;
}

/* the seed's index inflated into idx (cap >= 3 x 532 + 1); its length, 3 x 532 when the seed is sound */
static size_t ljb_seed_index(uint8_t *idx, size_t cap, const uint8_t *seed, size_t sn)
{
    const uint8_t *t = seed + sn - LJ_LJB_TRAILER;
    return tinfl_decompress_mem_to_mem(idx, cap, seed + rd32le(t + 16), rd32le(t + 12), TINFL_FLAG_PARSE_ZLIB_HEADER);
}

/* LORD_C-7 second mode: mutate the inflated index (kept at n x 532 so it passes the size rule), deflate it again, then
 * open and extract; the per-record checks behind the inflate are what this mode reaches. Some must open (the mode works). */
static int fuzz_ljb_index(const uint8_t *seed, size_t sn)
{
    static uint8_t idx[3 * LJ_LJB_INDEX_REC + 1], m[3 * LJ_LJB_INDEX_REC + 64], buf[16384];
    const size_t want = 3 * LJ_LJB_INDEX_REC;
    size_t n, mn, xn = 0;
    uint8_t *x = NULL;
    lj_ljb b;
    char err[128];
    uint32_t s = 0x2C9u, k;
    int i, bad = 0, opened = 0;
    if (ljb_seed_index(idx, sizeof idx, seed, sn) != want) { printf("FAIL fuzz_ljb index: seed index\n"); return 1; }
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(m, idx, want); mn = want;
        for (k = lj_fuzz_next(&s) % 8 + 1; k > 0; k--) lj_fuzz_mutate(m, &mn, sizeof m, &s);
        if (mn < want) memset(m + mn, 0, want - mn);
        if (!(n = ljb_reindex(buf, sizeof buf, seed, sn, m, want, 1))) { if (!bad++) printf("FAIL index iteration %d: re-deflate\n", i); continue; }
        if (lj_ljb_open(&b, buf, n, err, sizeof err) != 0) continue;
        opened++;
        for (k = 0; k < b.n; k++) {
            if (lj_ljb_extract(&b, k, &x, &xn, err, sizeof err) == 0 && xn != b.e[k].usz) { if (!bad++) printf("FAIL index iteration %d: entry %u size\n", i, (unsigned)k); }
            if (!lj_ljb_path_ok(b.e[k].folder, b.e[k].name)) { if (!bad++) printf("FAIL index iteration %d: entry %u path passed open\n", i, (unsigned)k); }
            free(x); x = NULL;
        }
        lj_ljb_free(&b);
    }
    if (!opened && !bad++) printf("FAIL fuzz_ljb index: no mutated index opened\n");
    printf("fuzz_ljb index: %d iterations, %d opened, %s\n", LJ_FUZZ_ITERS, opened, bad ? "FAIL" : "PASS");
    return bad;
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
        printf("FAIL fuzz_ljb: lj_ljb_open / lj_ljb_extract do not read the synthetic container\n");
        free(x);
        return 1;
    }
    free(x); x = NULL; lj_ljb_free(&b);
    if (argc >= 3 && strcmp(argv[1], "--seed-index") == 0) {   /* LORD_C-7: the fuzz_ljb_index corpus, its index stored raw */
        static uint8_t idx[3 * LJ_LJB_INDEX_REC + 1], raw[16384];
        static char opt[] = "--seed";
        char *a[3];
        size_t in = ljb_seed_index(idx, sizeof idx, seed, sn), rn;
        a[0] = argv[0]; a[1] = opt; a[2] = argv[2];
        rn = in == 3 * LJ_LJB_INDEX_REC ? ljb_reindex(raw, sizeof raw, seed, sn, idx, in, 0) : 0;
        return rn && lj_fuzz_seed(3, a, "show-index.LJB", raw, rn) == 1 ? 0 : 1;
    }
    if (lj_fuzz_seed(argc, argv, "show.LJB", seed, sn)) return 0;   /* ME-186 */
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, seed, sn); n = sn;
        for (k = lj_fuzz_next(&s) % 8 + 1; k > 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        if ((i & 1) && n > LJ_LJB_TRAILER) le32(buf + n - LJ_LJB_TRAILER, (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, n - LJ_LJB_TRAILER));
        if (lj_ljb_open(&b, buf, n, err, sizeof err) != 0) continue;
        for (k = 0; k < b.n; k++) {
            if (lj_ljb_extract(&b, k, &x, &xn, err, sizeof err) == 0 && xn != b.e[k].usz) { if (!bad++) printf("FAIL iteration %d: entry %u size\n", i, (unsigned)k); }
            if (!lj_ljb_path_ok(b.e[k].folder, b.e[k].name)) { if (!bad++) printf("FAIL iteration %d: entry %u path passed open\n", i, (unsigned)k); }
            free(x); x = NULL;
        }
        lj_ljb_free(&b);
    }
    printf("fuzz_ljb: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    bad += fuzz_ljb_index(seed, sn);
    return bad ? 1 : 0;
}

/* ---- W5-LJB-1 / -2: refusals ---- */

static const char *open_err(const uint8_t *d, size_t n)
{
    static char err[128];
    lj_ljb b;
    if (lj_ljb_open(&b, d, n, err, sizeof err) == 0) { lj_ljb_free(&b); return ""; }
    return err;
}

static const char *one(const ljb_ent *e, int n, uint8_t *buf, size_t cap, size_t *len)   /* pack + open */
{
    *len = ljb_pack(buf, cap, e, n);
    return *len ? open_err(buf, *len) : "pack failed";
}

static void test_container(void)
{
    static uint8_t buf[1 << 16];
    static const uint8_t hello[] = "hello, synthetic world";
    uint8_t *big;
    ljb_ent e[2];
    lj_ljb b;
    uint8_t *x = NULL;
    size_t n, xn;
    char err[128];
    memset(e, 0, sizeof e);
    e[0].name = "Config.dat"; e[0].folder = ""; e[0].data = hello; e[0].len = sizeof hello - 1;
    e[1] = e[0]; e[1].name = "Main.Ini"; e[1].stored = 1;
    CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "") == 0);
    buf[3] ^= 1;                                                        CHECK(strcmp(open_err(buf, n), LJ_LJB_ERR_CRC) == 0);   /* CRC first */
    buf[3] ^= 1;                                                        CHECK(strcmp(open_err(buf, n - 1), LJ_LJB_ERR_CRC) == 0);   /* truncated */
    CHECK(strcmp(open_err(buf, 10), LJ_LJB_ERR_CRC) == 0);
    le32(buf + n - 32, 0); recrc(buf, n);                               CHECK(strcmp(open_err(buf, n), LJ_LJB_ERR_INDEX) == 0);   /* n = 0 */
    le32(buf + n - 32, LJ_LJB_ENTRIES_MAX + 1); recrc(buf, n);          CHECK(strcmp(open_err(buf, n), LJ_LJB_ERR_INDEX) == 0);   /* n capped */
    le32(buf + n - 32, 1); recrc(buf, n);                               CHECK(strcmp(open_err(buf, n), LJ_LJB_ERR_INDEX) == 0);   /* inflated index != n x 532 */
    le32(buf + n - 32, 2); le32(buf + n - 24, 0xFFFFF000u); recrc(buf, n); CHECK(strcmp(open_err(buf, n), LJ_LJB_ERR_INDEX) == 0);   /* ioff + isz past len - 36 */
    CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "") == 0);
    le32(buf + n - 20, (uint32_t)n); recrc(buf, n);                               CHECK(strcmp(open_err(buf, n), LJ_LJB_ERR_INDEX) == 0);   /* ioff past the end */

    e[0].flags = 0x2;                                                   CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: bad entry 0") == 0);   /* other flags refused */
    e[0].flags = 0; e[1].usz = 5;                                       CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: bad entry 1") == 0);   /* 0x1 needs csz == usz */
    e[1].usz = 0; e[0].off = 60000;                                     CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: bad entry 0") == 0);   /* off + csz <= ioff */
    e[0].off = 0; e[0].usz = 100000;                                    CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: bad entry 0") == 0);   /* more than 1032 x csz */
    e[0].usz = 10;                                                      /* claims less than it inflates to: capped at usz, refused at extract */
    CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "") == 0 && lj_ljb_open(&b, buf, n, err, sizeof err) == 0);
    CHECK(lj_ljb_extract(&b, 0, &x, &xn, err, sizeof err) == -1 && x == NULL && strcmp(err, "LJB: bad entry 0") == 0);
    CHECK(lj_ljb_extract(&b, 1, &x, &xn, err, sizeof err) == 0 && xn == sizeof hello - 1 && memcmp(x, hello, xn) == 0);
    free(x); x = NULL;                                                  /* the next call sets x to NULL */
    CHECK(lj_ljb_extract(&b, 2, &x, &xn, err, sizeof err) == -1);   /* index out of range */
    free(x); x = NULL; lj_ljb_free(&b);
    e[0].usz = 0;
    {   /* entry CRC */
        uint32_t off = 0;
        CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "") == 0 && lj_ljb_open(&b, buf, n, err, sizeof err) == 0);
        if (b.n == 2) off = b.e[1].off;                                 /* the stored entry's bytes */
        lj_ljb_free(&b);
        buf[off + 2] ^= 0x40; recrc(buf, n);
        CHECK(lj_ljb_open(&b, buf, n, err, sizeof err) == 0);
        if (b.n == 2) CHECK(lj_ljb_extract(&b, 1, &x, &xn, err, sizeof err) == -1 && strcmp(err, "LJB: bad entry 1") == 0);
        free(x); x = NULL; lj_ljb_free(&b);
    }
    /* the total cap before any allocation: one deflate entry claims 0x80000000 bytes from 2 MB of stored bytes */
    big = (uint8_t *)calloc(1, (2u << 20) + 4096);
    if (big) {
        uint8_t idx[LJ_LJB_INDEX_REC];
        mz_ulong zl = 4000;
        size_t dl = 2u << 20;
        memset(idx, 0, sizeof idx); sstr_put(idx, "Config.dat");
        le32(idx + 512, 0x80000000u); le32(idx + 516, (uint32_t)dl); le32(idx + 520, 0); le32(idx + 528, LJ_LJB_FLAG_DEFLATE);
        CHECK(mz_compress2(big + dl, &zl, idx, sizeof idx, 9) == MZ_OK);
        le32(big + dl + zl + 4, 1); le32(big + dl + zl + 12, (uint32_t)zl); le32(big + dl + zl + 16, (uint32_t)dl);
        recrc(big, dl + zl + LJ_LJB_TRAILER);
        CHECK(strcmp(open_err(big, dl + zl + LJ_LJB_TRAILER), LJ_LJB_ERR_SIZE) == 0);
        free(big);
    }
    /* duplicates, case-insensitively; hostile names (W5-LJB-2) */
    e[1].name = "config.DAT"; e[1].stored = 0;                          CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: refused path in entry 1") == 0);
    e[1].name = "Main.Ini"; e[1].folder = "..\\";                        CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: refused path in entry 1") == 0);
    e[1].folder = "C:\\";                                               CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: refused path in entry 1") == 0);
    e[1].folder = ""; e[1].name = "CON";                                CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: refused path in entry 1") == 0);
    e[1].name = "Main.Ini"; e[1].namelen = 9;                           CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: refused path in entry 1") == 0);   /* the NUL after the name counted in */
    e[1].namelen = 0; e[1].name = "";                                   CHECK(strcmp(one(e, 2, buf, sizeof buf, &n), "LJB: refused path in entry 1") == 0);   /* empty name */

    CHECK(lj_ljb_path_ok("", "Config.dat") && lj_ljb_path_ok("Sequences\\", "00000001.seq") && lj_ljb_path_ok("LJMData\\Loops\\", "Loop0001.LSL"));
    CHECK(lj_ljb_path_ok("", "Info.$$$") && lj_ljb_path_ok("UserFixtureDef\\", "FixtureDef.41") && lj_ljb_path_ok("", "CONSOLE.txt"));
    CHECK(!lj_ljb_path_ok("..\\", "a") && !lj_ljb_path_ok(".\\", "a") && !lj_ljb_path_ok("a\\..\\", "b") && !lj_ljb_path_ok("", ".."));
    CHECK(!lj_ljb_path_ok("\\", "a") && !lj_ljb_path_ok("\\abs\\", "a") && !lj_ljb_path_ok("a\\\\", "b") && !lj_ljb_path_ok("", ""));
    CHECK(!lj_ljb_path_ok("C:\\", "a") && !lj_ljb_path_ok("", "a:b") && !lj_ljb_path_ok("", "a/b") && !lj_ljb_path_ok("a/", "b") && !lj_ljb_path_ok("a", "b"));
    CHECK(!lj_ljb_path_ok("", "CON") && !lj_ljb_path_ok("", "con.txt") && !lj_ljb_path_ok("", "Nul") && !lj_ljb_path_ok("LPT1\\", "a") && !lj_ljb_path_ok("", "com9.ini"));
    CHECK(!lj_ljb_path_ok("", "a.") && !lj_ljb_path_ok("", "a ") && !lj_ljb_path_ok("", "a\tb") && !lj_ljb_path_ok("", "a*b") && !lj_ljb_path_ok("", "a?"));
    /* LORD_C-8: CP1252 superscript digits are COM / LPT digits; spaces before the extension leave the device name */
    CHECK(!lj_ljb_path_ok("", "COM\xB9.txt") && !lj_ljb_path_ok("", "lpt\xB2") && !lj_ljb_path_ok("COM\xB3\\", "a") && !lj_ljb_path_ok("", "CON .txt") && !lj_ljb_path_ok("", "com1  .ini"));
    CHECK(lj_ljb_path_ok("", "COM\xB4.txt") && lj_ljb_path_ok("", "CONX .txt"));
    printf("ljb container: %d failures after the refusal rows\n", fails);
}

/* ---- W5-LJB-3 / -4: a synthetic library of every converted kind ---- */

static uint8_t g_config[16546], g_groups[12972], g_seq[0x18C8 + 2 * 6312], g_badseq[0x18C8 + 2 * 6312], g_def[2748], g_stat[127342];
static uint8_t g_cma[26659], g_cue[1132], g_pages[6 + 2 * 35], g_info[1548], g_gs2[LJ_GS2_REC_SIZE], g_cl2[4135];
static const char g_notes[] = "{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}\\f0 hello\\par world\\par}";

static void cfg_fx(int slot, unsigned type, unsigned code, const char *user, int x, int y, int tab)
{
    uint8_t *r = g_config + 0x15 + (slot - 1) * 165;
    le16(r, type); le16(r + 2, code); sstr_put(r + 0x0B, user); r[0x20] = 1; le32(r + 0x21, (uint32_t)x); le32(r + 0x25, (uint32_t)y); r[0x29] = (uint8_t)tab;
}

static int build_library(ljb_ent *e)   /* returns the entry count */
{
    int n = 0;
    uint8_t *s;
    memset(g_config, 0, sizeof g_config);
    g_config[0] = 0xDE;
    cfg_fx(1, 68, 0, "MAC2000", 8, 8, 1);            /* MAC 2000 at 1:001 */
    cfg_fx(2, 133, 600, "Wash", 90, 8, 2);           /* MAC 250 Wash at 2:089 */
    cfg_fx(3, 32, 100, "nothing", 0, 0, 1);          /* exe row 32 is the empty slot: no profile, skipped (never renumbered) */
    cfg_fx(4, 46, 3000, "far", 0, 0, 1);             /* code past 2047, skipped */
    cfg_fx(5, 46, 1024, "DMX10", 172, 92, 4);        /* user-defined: FixtureDef.4 below */
    memset(g_groups, 0, sizeof g_groups); g_groups[0] = 0xDE;
    sstr_put(g_groups + 2, "Front"); g_groups[2 + 31 + 0] = 1; g_groups[2 + 31 + 1] = 1;
    memset(g_seq, 0, sizeof g_seq);
    sstr_put(g_seq + 0x10, "Chase"); g_seq[0x44] = 0xDE; sstr_put(g_seq + 0x46, "00000003.seq"); g_seq[0x5B] = 2;
    s = g_seq + 0x18C8; le32(s, 1000); le32(s + 4, 50); s[0x10] = 1; s[0x10 + 1] = 2; s[0xADC] = 49; s[0xADC + 1] = 255;
    s += 6312; le32(s, 460); le32(s + 4, 2000); s[0x10] = 2; s[0xADC] = 10;
    memcpy(g_badseq, g_seq, sizeof g_badseq); g_badseq[0x5B] = 1;
    memset(g_def, 0, sizeof g_def); g_def[1] = 255; g_def[600 + 7] = 0x40;
    memset(g_stat, 0, sizeof g_stat); g_stat[0] = 0xDE;
    s = g_stat + 2; le32(s, 1000); le32(s + 4, 300); s[0x10 + 5] = 2; s[0xADC + 5] = 200;
    sstr_put(g_stat + 2 + 20 * 6312 + 40, "RGBAW Int"); sstr_put(g_stat + 2 + 20 * 6312 + 40 + 31, "default");
    memset(g_cma, 0, sizeof g_cma); sstr_put(g_cma + 4, "Fan");
    s = g_cma + 4131 + 120 * 11; s[0] = 17; s[1] = 1; s[2] = 0; le16(s + 3, 255); le16(s + 5, 255); le16(s + 7, 55); le16(s + 9, 16);
    memset(g_cue, 0, sizeof g_cue); sstr_put(g_cue, "Default Values"); sstr_put(g_cue + 0x1F, "00000001.cue"); le32(g_cue + 0x90, 3); memset(g_cue + 0x95, 0xFF, 8); g_cue[0x118] = 1;
    memset(g_pages, 0, sizeof g_pages); g_pages[0] = 0xDE; le32(g_pages + 2, 2); sstr_put(g_pages + 6, "Page 1"); sstr_put(g_pages + 6 + 35, "Encore");
    memset(g_pages + 6 + 7, 0xEC, 5);                /* stale padding after "Page 1" (18 s3): ignored */
    memset(g_info, 0, sizeof g_info); le32(g_info, 1); sstr_put(g_info + 4, "X:\\synthetic-private-root\\User0009\\"); sstr_put(g_info + 0x104, " My:Show*\x01. ");
    memset(g_cl2, 0, sizeof g_cl2); le32(g_cl2, 1); sstr_put(g_cl2 + 4, "AutoSave");
    CHECK(lj_fixlib_by_id(19) && lj_gs2_write(lj_fixlib_by_id(19), g_gs2) == 0);   /* our exporter's record of the built-in dimmer: no Martin bytes */
#define ENT(nm, fd, buf, len_, st) do { memset(&e[n], 0, sizeof e[n]); e[n].name = nm; e[n].folder = fd; e[n].data = (const uint8_t *)(buf); e[n].len = len_; e[n].stored = st; n++; } while (0)
    ENT("Info.$$$", "", g_info, sizeof g_info, 0);
    ENT("Notes.$$$", "", g_notes, sizeof g_notes - 1, 0);
    ENT("Config.dat", "", g_config, sizeof g_config, 0);
    ENT("AutoSave.Cl2", "CueLists\\", g_cl2, sizeof g_cl2, 0);
    ENT("00000002.CMA", "CueMacros\\", g_cma, sizeof g_cma, 0);
    ENT("00000001.cue", "Cues\\", g_cue, sizeof g_cue, 0);
    ENT("CuePageInfo.Dat", "Cues\\", g_pages, sizeof g_pages, 0);
    ENT("DefaultOut.dat", "", g_def, sizeof g_def, 0);
    ENT("DirInfo.Ini", "", "[INFO]\r\nName=Ignored\r\n", 22, 1);
    ENT("GroupInfo.dat", "", g_groups, sizeof g_groups, 0);
    ENT("Loop0001.LSL", "LJMData\\Loops\\", "ljm", 3, 0);
    ENT("Selection.Dat", "", "", 0, 1);
    ENT("00000003.seq", "Sequences\\", g_seq, sizeof g_seq, 0);
    ENT("00000004.seq", "Sequences\\", g_badseq, sizeof g_badseq, 0);
    ENT("Special.dat", "", "\0\0\0", 3, 0);
    ENT("Statics.dat", "", g_stat, sizeof g_stat, 0);
    ENT("FixtureDef.4", "UserFixtureDef\\", g_gs2, sizeof g_gs2, 0);
    ENT("Setup.43", "UserFixtureDef\\", "pal", 3, 0);
#undef ENT
    return n;
}

static int has_line(const lj_ljb_report *r, const char *start)
{
    int i;
    for (i = 0; i < r->nlines; i++) if (strncmp(r->lines[i], start, strlen(start)) == 0) return 1;
    return 0;
}
static int file_has(const char *path, const char *text)
{
    static char buf[1 << 16];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return -1;
    n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    return strstr(buf, text) != NULL;
}
static int write_file(const char *path, const uint8_t *d, size_t n) { FILE *f = fopen(path, "wb"); if (!f) return -1; fwrite(d, 1, n, f); return fclose(f); }

static size_t g_ljb_len;
static uint8_t *g_ljb;

static void test_import(void)
{
    static ljb_ent e[32];
    static lj_sequence q;
    static lj_cue cue;
    static lj_genmacro m;
    static lj_cuepages pg;
    static lj_ljb_report rep;
    char path[LJ_PATH_CAP], err[160], notes[256], names[64][LJ_LIBNAME_CAP];
    lj_library lib, lib2;
    lj_show *show;
    lj_ini *ini = NULL;
    int n = build_library(e), before;
    g_ljb = (uint8_t *)malloc(1 << 20);
    if (!g_ljb) return;
    g_ljb_len = ljb_pack(g_ljb, 1 << 20, e, n);
    CHECK(g_ljb_len > 0 && write_file(TMP "Show.LJB", g_ljb, g_ljb_len) == 0);
    lj_library_delete(TMP, "MyShow"); lj_library_delete(TMP, "MyShow (2)"); lj_library_delete(TMP, "Default (2)");

    CHECK(lj_ljb_preview(TMP "Show.LJB", path, LJ_LIBNAME_CAP, notes, sizeof notes, err, sizeof err) == 0 && strcmp(path, "MyShow") == 0);   /* Info.$$$ sanitised */
    CHECK(strstr(notes, "hello") && strstr(notes, "world") && !strstr(notes, "rtf"));
    CHECK(lj_ljb_import(TMP "Show.LJB", TMP, NULL, &lib, &rep, err, sizeof err) == 0);
    if (fails) printf("import: %s\n", err);
    CHECK(strcmp(lib.name, "MyShow") == 0);
    CHECK(rep.converted == 9);   /* Config GroupInfo DefaultOut Statics CuePageInfo .seq .cue .CMA FixtureDef */
    CHECK(rep.skipped == 7);     /* AutoSave.Cl2 DirInfo.Ini Loop0001.LSL Selection.Dat 00000004.seq Special.dat Setup.43 */
    CHECK(has_line(&rep, "CueLists\\AutoSave.Cl2: cue list not converted"));
    CHECK(has_line(&rep, "Cues\\00000001.cue: slots 2-12 and the cue options not converted"));
    CHECK(has_line(&rep, "Sequences\\00000004.seq: size does not match the scene count"));
    CHECK(has_line(&rep, "Special.dat: not converted (contents undecoded"));
    CHECK(has_line(&rep, "UserFixtureDef\\Setup.43: custom palettes not converted"));
    CHECK(has_line(&rep, "Config.dat: fixture 3: type 32 has no LJ4EVR profile"));
    CHECK(has_line(&rep, "Config.dat: fixture 4: address code 3000 not 0..2047"));
    CHECK(has_line(&rep, "LJMData\\: 1 files not converted"));
    CHECK(lj_library_path(&lib, LJ_LJB_REPORT, path, sizeof path) == 0 && file_has(path, "cue list not converted") == 1 && file_has(path, "synthetic-private-root") == 0);
    CHECK(lj_library_path(&lib, "DirInfo.Ini", path, sizeof path) == 0 && (ini = lj_ini_load(path)) != NULL);
    if (ini) { CHECK(lj_ini_geti(ini, "NEXT", "Seq", 0) == 4 && lj_ini_geti(ini, "NEXT", "Cue", 0) == 2 && lj_ini_geti(ini, "NEXT", "Macro", 0) == 3 && lj_ini_geti(ini, "NEXT", "Cl", 0) == 1); lj_ini_free(ini); }

    /* every converted record read back through the LJ4 loaders */
    show = lj_show_new();
    CHECK(lj_show_load(show, &lib) == 0);
    CHECK(show->fixture[0].kind == LJ_FX_BUILTIN && show->fixture[0].profile_id == 68 && show->fixture[0].link == 1 && show->fixture[0].address == 1);   /* ME-8 */
    CHECK(strcmp(show->fixture[0].user_name, "MAC2000") == 0 && show->fixture[0].placed && show->fixture[0].icon_x == 8 && show->fixture[0].tab == 0);
    CHECK(show->fixture[1].profile_id == 133 && show->fixture[1].link == 2 && show->fixture[1].address == 89 && show->fixture[1].tab == 1);
    CHECK(show->fixture[2].link == 0 && show->fixture[3].link == 0);
    CHECK(show->fixture[4].kind == LJ_FX_GS2 && strcmp(show->fixture[4].gs2_file, "LJBSlot005.GS2") == 0 && show->fixture[4].link == 3 && show->fixture[4].address == 1 && show->fixture[4].icon_y == 92 && show->fixture[4].tab == 3);
    CHECK(show->default_out[1] == 255 && show->default_out[607] == 0x40);
    CHECK(strcmp(show->user_group[0].name, "Front") == 0 && show->user_group[0].member[0] && show->user_group[0].member[1] && !show->user_group[0].member[2]);
    CHECK(show->npages == 1);
    if (show->npages == 1) {
        lj_static_slot *s0 = &show->pages[0].slot[0], *s1 = &show->pages[0].slot[1];
        CHECK(strcmp(s0->name, "RGBAW Int") == 0 && s0->kind == LJ_STATIC_RECORDED && s0->fade_ms == 300 && s0->level_pct == 100 && s0->scene && s0->scene->state[5] == LJ_FADE && s0->scene->val[5] == 200);
        CHECK(strcmp(s1->name, "default") == 0 && s1->kind == LJ_STATIC_EMPTY);
    }
    lj_show_free(show);
    CHECK(lj_library_path(&lib, "UserFixtureDef/LJBSlot005.GS2", path, sizeof path) == 0 && file_has(path, "") == 1);
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 3, path, sizeof path) == 0 && lj_sequence_load(path, &q, err, sizeof err) == 0);
    CHECK(q.number == 3 && strcmp(q.name, "Chase") == 0 && q.n == 2);
    if (q.n == 2) {
        CHECK(q.scenes[0].scene_ms == 1000 && q.scenes[0].fade_ms == 50 && q.scenes[0].state[0] == LJ_SNAP && q.scenes[0].val[0] == 49 && q.scenes[0].state[1] == LJ_FADE && q.scenes[0].val[1] == 255);
        CHECK(q.scenes[1].scene_ms == 460 && q.scenes[1].fade_ms == 2000 && q.scenes[1].state[0] == LJ_FADE && q.scenes[1].val[0] == 10 && q.scenes[1].state[1] == LJ_OFF);
    }
    lj_sequence_free_scenes(&q);
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 4, path, sizeof path) == 0 && file_has(path, "") == -1);   /* the bad-size sequence is not written */
    CHECK(lj_library_record_path(&lib, LJ_REC_CUE, 1, path, sizeof path) == 0 && lj_cue_load(path, &cue, err, sizeof err) == 0);
    CHECK(strcmp(cue.name, "Default Values") == 0 && cue.slot[0].seq == 3 && cue.slot[0].enabled && cue.slot[0].trig == LJ_TRIG_AUTO && cue.slot[1].seq == 0);
    CHECK(lj_library_record_path(&lib, LJ_REC_MACRO, 2, path, sizeof path) == 0 && lj_genmacro_load(path, &m, err, sizeof err) == 0);
    CHECK(strcmp(m.name, "Fan") == 0 && m.ch[120].shape == 17 && m.ch[120].base_on == 1 && m.ch[120].amp_pos == 255 && m.ch[120].amp_neg == 255 && m.ch[120].delay == 55 && m.ch[120].cycle_ds == 16);
    CHECK(!lj_genmacro_ch_used(&m, 119) && !lj_genmacro_ch_used(&m, 121));
    CHECK(lj_library_path(&lib, "Cues\\CuePageInfo.Dat", path, sizeof path) == 0 && lj_cuepages_load(path, &pg, err, sizeof err) == 0);
    CHECK(pg.n == 2 && strcmp(pg.page[0].name, "Page 1") == 0 && strcmp(pg.page[1].name, "Encore") == 0 && pg.page[0].n == 0);
    CHECK(lj_library_path(&lib, LJ_NOTES_FILE, path, sizeof path) == 0 && lj_notes_load(path, notes, sizeof notes) == 0 && strstr(notes, "hello") && strstr(notes, "world"));

    /* never an existing library: a second import gets "MyShow (2)", the first is untouched; "Default" is never taken */
    CHECK(lj_ljb_import(TMP "Show.LJB", TMP, NULL, &lib2, &rep, err, sizeof err) == 0 && strcmp(lib2.name, "MyShow (2)") == 0);
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 3, path, sizeof path) == 0 && lj_sequence_load(path, &q, err, sizeof err) == 0 && q.n == 2);
    lj_sequence_free_scenes(&q);
    CHECK(lj_ljb_import(TMP "Show.LJB", TMP, "Default", &lib2, &rep, err, sizeof err) == 0 && strcmp(lib2.name, "Default (2)") == 0);
    lj_library_delete(TMP, "MyShow (2)"); lj_library_delete(TMP, "Default (2)");

    /* a hostile file under the scratch root: refused with the message, nothing written, no library left */
    before = lj_library_list(TMP, names, 64);
    e[n - 1].folder = "..\\..\\";
    e[n - 1].name = "evil.txt";
    {   uint8_t *ev = (uint8_t *)malloc(1 << 20);
        size_t hn = ev ? ljb_pack(ev, 1 << 20, e, n) : 0;
        CHECK(hn && write_file(TMP "Evil.LJB", ev, hn) == 0);
        CHECK(lj_ljb_import(TMP "Evil.LJB", TMP, NULL, &lib2, &rep, err, sizeof err) == -1 && strncmp(err, "LJB: refused path in entry", 26) == 0);
        CHECK(lj_library_list(TMP, names, 64) == before && file_has("evil.txt", "") == -1 && file_has(TMP "evil.txt", "") == -1);
        if (ev) ev[100] ^= 0x10;
        CHECK(ev && write_file(TMP "Evil.LJB", ev, hn) == 0 && lj_ljb_import(TMP "Evil.LJB", TMP, NULL, &lib2, &rep, err, sizeof err) == -1 && strcmp(err, LJ_LJB_ERR_CRC) == 0);
        CHECK(lj_library_list(TMP, names, 64) == before);
        free(ev);
    }
    CHECK(lj_ljb_import(TMP "missing.LJB", TMP, NULL, &lib2, &rep, err, sizeof err) == -1 && strcmp(err, LJ_LJB_ERR_READ) == 0);
    printf("ljb import: %d converted, %d not converted, %d failures so far\n", 9, 7, fails);
}

/* ---- W5-LJB-4: the Restore window entry, headless ---- */

static int act_select = -1;
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)ud; if (a == LJ_ACT_SELECT_LIBRARY) act_select = arg; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static const char *cap(lj_widget *w) { const char *c = w ? lj_w_caption(w) : NULL; return c ? c : ""; }
static const char *msg_line(const char *label) { lj_window *m = lj_win_message_window(); return m ? cap(lj_window_find(m, label)) : ""; }
static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); click(m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL); }
static int memo_has(lj_window *w, const char *start)
{
    lj_w_memo *m = (lj_w_memo *)(w ? lj_window_find(w, "Re1") : NULL);
    int i;
    for (i = 0; m && i < m->n; i++) if (strncmp(m->lines[i], start, strlen(start)) == 0) return 1;
    return 0;
}

static void test_restore_window(void)
{
    static lj_library lib;
    lj_ctx *c = lj_ctx_get();
    char names[64][LJ_LIBNAME_CAP];
    lj_window *w;
    lj_widget *p, *k;
    lj_w_list *files = NULL;
    lj_library got;
    lj_show *show;
    int i, n;
    lj_library_delete(TMP, "MyShow");
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { fails++; printf("FAIL cannot create Lib\n"); return; }
    if (snprintf(c->userdir, sizeof c->userdir, TMP) < 0) c->userdir[0] = 0;
    c->lib = &lib; c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    lj_desktop_bind_action(LJ_ACT_SELECT_LIBRARY, on_action, NULL);
    CHECK(write_file(TMP "Show.LJB", g_ljb ? g_ljb : (const uint8_t *)"", g_ljb_len) == 0);
    w = lj_win_open(LJ_WIN_RESTORE);
    CHECK(w != NULL);
    if (!w) return;
    CHECK(lj_restore_set_dir(TMP) == 0);
    p = lj_window_find(w, "FileList") ? lj_window_find(w, "FileList")->parent : NULL;
    for (k = p ? p->first : NULL; k; k = k->next) if (!k->name && k->visible) files = (lj_w_list *)k;
    CHECK(files != NULL);
    for (i = 0; files && i < files->n; i++) if (strcmp(files->items[i], "Show.LJB") == 0) break;
    CHECK(files && i < files->n);                                       /* *.LJB listed beside *.lj4b */
    CHECK(lj_restore_set_file(TMP "Show.LJB") == 0 && strcmp(cap(lj_window_find(w, "lLibName")), "MyShow") == 0);
    CHECK(memo_has(w, "hello"));                                        /* Notes.$$$ stripped into the notes box */
    CHECK(lj_restore_run() == 0 && strcmp(msg_line("Label1"), "Import \"MyShow\" as a new user library ?") == 0);
    msg_answer(1);
    CHECK(memo_has(w, "Imported into \"MyShow\": 9 converted, 7 not converted"));
    CHECK(memo_has(w, "CueLists\\AutoSave.Cl2: cue list not converted"));   /* the report the operator sees */
    CHECK(strcmp(msg_line("Label1"), LJ_USERLIB_RESTART1) == 0);
    msg_answer(1);
    n = lj_library_list(TMP, names, 64);
    CHECK(act_select >= 0 && act_select < n && strcmp(names[act_select], "MyShow") == 0);   /* the app selects it and restarts (LJ_ACT_SELECT_LIBRARY) */
    show = lj_show_new();
    CHECK(lj_library_open(TMP, "MyShow", &got) == 0 && lj_show_load(show, &got) == 0 && show->fixture[0].profile_id == 68);   /* the library opens */
    lj_show_free(show);
    /* a hostile file through the same entry: the message, no library */
    g_ljb[50] ^= 1;
    CHECK(write_file(TMP "Bad.LJB", g_ljb, g_ljb_len) == 0);
    g_ljb[50] ^= 1;
    CHECK(lj_restore_set_file(TMP "Bad.LJB") == 0 && memo_has(w, LJ_LJB_ERR_CRC));
    CHECK(lj_restore_run() == 0);
    msg_answer(1);
    CHECK(strcmp(msg_line("Label1"), LJ_LJB_ERR_CRC) == 0 || strcmp(msg_line("Label2"), LJ_LJB_ERR_CRC) == 0);
    msg_answer(1);
    CHECK(lj_library_list(TMP, names, 64) == n);
    lj_win_close(LJ_WIN_RESTORE);
    lj_show_free(c->show); lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini);
    c->show = NULL; c->main_ini = NULL; c->forms_ini = NULL; c->lib = NULL;
    lj_library_delete(TMP, "MyShow"); lj_library_delete(TMP, "Lib");
}

/* ---- LORD_C-9: a .lj4b notes.txt is size-capped before it is extracted ---- */

static void test_notes_cap(void)
{
    mz_zip_archive z;
    char *big = (char *)malloc(LJ_NOTES_CAP + 2), notes[64];
    if (!big) return;
    memset(big, 'n', LJ_NOTES_CAP + 1);
    memset(&z, 0, sizeof z);
    CHECK(mz_zip_writer_init_file(&z, TMP "big.lj4b", 0) && mz_zip_writer_add_mem(&z, "notes.txt", big, LJ_NOTES_CAP + 1, MZ_DEFAULT_LEVEL));
    mz_zip_writer_finalize_archive(&z); mz_zip_writer_end(&z);
    CHECK(lj_library_backup_notes(TMP "big.lj4b", notes, sizeof notes) == -1);   /* oversize refused */
    memset(&z, 0, sizeof z);
    CHECK(mz_zip_writer_init_file(&z, TMP "ok.lj4b", 0) && mz_zip_writer_add_mem(&z, "notes.txt", big, LJ_NOTES_CAP, MZ_DEFAULT_LEVEL));
    mz_zip_writer_finalize_archive(&z); mz_zip_writer_end(&z);
    CHECK(lj_library_backup_notes(TMP "ok.lj4b", notes, sizeof notes) == 0 && strlen(notes) == sizeof notes - 1);   /* at the cap: read, cut to the box */
    remove(TMP "big.lj4b"); remove(TMP "ok.lj4b");
    free(big);
}

int lj_test_ljb(int argc, char **argv);
/* lj4evr_tests test_ljb --write <dir>: the synthetic Show.LJB of test_import, plus Evil.LJB (an entry "..\..\evil.txt")
 * and Bad.LJB (one flipped byte, CRC refused), for the W5 acceptance walk (docs/w5-integration.md "Acceptance" row 3) */
static int write_samples(const char *dir)
{
    static ljb_ent e[32];
    char path[LJ_PATH_CAP];
    size_t len;
    int n = build_library(e), bad = 0;
    uint8_t *b = (uint8_t *)malloc(1 << 20);
    if (!b) return 1;
    len = ljb_pack(b, 1 << 20, e, n);
    bad |= snprintf(path, sizeof path, "%s/Show.LJB", dir) < 0 || !len || write_file(path, b, len) != 0;
    b[50] ^= 1;
    bad |= snprintf(path, sizeof path, "%s/Bad.LJB", dir) < 0 || write_file(path, b, len) != 0;
    e[n - 1].folder = "..\\..\\"; e[n - 1].name = "evil.txt";
    len = ljb_pack(b, 1 << 20, e, n);
    bad |= snprintf(path, sizeof path, "%s/Evil.LJB", dir) < 0 || !len || write_file(path, b, len) != 0;
    free(b);
    printf("test_ljb --write: Show.LJB, Bad.LJB, Evil.LJB %s\n", bad ? "FAILED" : "written");
    return bad || fails;
}

int lj_test_ljb(int argc, char **argv)
{
    fails = 0;
    if (argc == 3 && strcmp(argv[1], "--write") == 0) return write_samples(argv[2]);
    lj_log_set_sink(NULL, NULL);
    lj_fs_mkdir(TMP);
    test_container();
    test_import();
    test_restore_window();
    test_notes_cap();
    remove(TMP "Show.LJB"); remove(TMP "Evil.LJB"); remove(TMP "Bad.LJB");
    lj_library_delete(TMP, "MyShow");
    free(g_ljb); g_ljb = NULL;
    printf("test_ljb: %s (%d)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}

/* ---- W5-LJB-5: the note 18 sample, developer PC only (counts printed, never a path or a name) ---- */

int lj_test_ljb_sample(int argc, char **argv);
int lj_test_ljb_sample(int argc, char **argv)
{
    static lj_ljb_report rep;
    char dir[LJ_PATH_CAP], names[4][LJ_DIRENT_CAP], path[LJ_PATH_CAP], err[160];
    uint8_t *d, *x;
    size_t len, xn;
    long sz;
    lj_ljb b;
    lj_library lib;
    lj_show *show;
    FILE *f;
    uint32_t i, ok = 0;
    int nf, k, fx = 0, groups = 0, statics = 0, skipped_fx;
    (void)argc; (void)argv;
    fails = 0;
    if (!LJ4EVR_REFERENCE_DIR[0] || snprintf(dir, sizeof dir, "%s/notes/ledtrix", LJ4EVR_REFERENCE_DIR) >= (int)sizeof dir
        || (nf = lj_library_dir_list(dir, ".LJB", names, 4)) < 1 || snprintf(path, sizeof path, "%s/%s", dir, names[0]) >= (int)sizeof path) {
        printf("ljb_sample: SKIP (no reference sample on this machine)\n");
        return 0;
    }
    if (!(f = fopen(path, "rb")) || fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0) { if (f) fclose(f); printf("FAIL ljb_sample: unreadable\n"); return 1; }
    len = (size_t)sz;
    d = (uint8_t *)malloc(len);
    CHECK(d && fread(d, 1, len, f) == len);
    fclose(f);
    if (!d) return 1;
    CHECK(lj_ljb_open(&b, d, len, err, sizeof err) == 0 && b.n == 127);
    for (i = 0; i < b.n; i++) if (lj_ljb_extract(&b, i, &x, &xn, err, sizeof err) == 0 && xn == b.e[i].usz) { ok++; free(x); }
    CHECK(ok == 127);
    printf("ljb_sample: %u entries, %u match size and CRC\n", (unsigned)b.n, (unsigned)ok);
    lj_ljb_free(&b);
    free(d);
    lj_fs_mkdir(TMP);
    lj_library_delete(TMP, "ljb_sample");
    CHECK(lj_ljb_import(path, TMP, "ljb_sample", &lib, &rep, err, sizeof err) == 0);
    show = lj_show_new();
    CHECK(lj_show_load(show, &lib) == 0);
    for (k = 0; k < LJ_FIXTURES; k++) fx += show->fixture[k].link != 0;
    for (k = 0; k < LJ_USER_GROUPS; k++) groups += show->user_group[k].name[0] != 0;
    for (k = 0; show->npages && k < 20; k++) statics += show->pages[0].slot[k].name[0] != 0;
    printf("ljb_sample: %d converted, %d not converted; %d fixtures, %d named groups, %d named statics\n", rep.converted, rep.skipped, fx, groups, statics);
    skipped_fx = 0;   /* counted in LJBImport.txt: the report struct keeps the first 64 lines only */
    if (lj_library_path(&lib, LJ_LJB_REPORT, path, sizeof path) == 0 && (f = fopen(path, "rb")) != NULL) {
        char line[512];
        while (fgets(line, sizeof line, f)) skipped_fx += strstr(line, "has no LJ4EVR profile") != NULL;
        fclose(f);
    }
    printf("ljb_sample: %d fixtures reported without a profile\n", skipped_fx);
    CHECK(fx == 53 && skipped_fx == 20 && groups == 2 && statics == 2);   /* note 18 s8: 73 patched = 53 + the 20 Stagebar 54 (ids 147, 151: exe rows with no LJ4EVR profile, reported, never renumbered); s6 two named statics */
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 1, path, sizeof path) == 0 && file_has(path, "LJ4 seq") == 1);
    CHECK(lj_library_record_path(&lib, LJ_REC_CUE, 1, path, sizeof path) == 0 && file_has(path, "LJ4 cue") == 1);
    CHECK(lj_library_record_path(&lib, LJ_REC_MACRO, 1, path, sizeof path) == 0 && file_has(path, "LJ4 macro") == 1);
    lj_show_free(show);
    lj_library_delete(TMP, "ljb_sample");
    printf("ljb_sample: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
