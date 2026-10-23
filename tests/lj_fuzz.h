/* lj_fuzz.h - the seeded mutator every W5 parser fuzz row shares (W5-X-HOSTILE; docs/w5-contracts.md seam 11): a fixed
 * xorshift32 seed and a fixed iteration count, so a failing input is reproducible from the row's printed iteration.
 * Each row first proves its synthetic seed parses (else it prints FAIL and returns 1), then mutates it LJ_FUZZ_ITERS
 * times through the real entry point
 * under ASan / UBSan (linux-gcc-asan). Header-only: one static helper set per test file. */
#ifndef LJ_TESTS_FUZZ_H
#define LJ_TESTS_FUZZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/lj_fsinfo.h"

#ifndef LJ_FUZZ_N   /* a local long run: build with -DLJ_FUZZ_N=100000 (docs/w5-integration.md) */
#define LJ_FUZZ_N 20000
#endif
enum { LJ_FUZZ_ITERS = LJ_FUZZ_N };

static inline uint32_t lj_fuzz_next(uint32_t *s) { uint32_t x = *s ? *s : 0x9E3779B9u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }

/* one mutation of buf[0..*n) in place, *n stays <= cap: flip a bit, set a byte, set a byte to an edge value, truncate,
 * extend with random bytes, or copy a run inside the buffer. */
static inline void lj_fuzz_mutate(uint8_t *buf, size_t *n, size_t cap, uint32_t *s)
{
    static const uint8_t edge[] = { 0x00, 0x01, 0x7F, 0x80, 0xFF, 0xFE, 0x10, 0x20 };
    uint32_t r = lj_fuzz_next(s);
    size_t i = *n ? lj_fuzz_next(s) % *n : 0, k;
    switch (r % 6) {
    case 0: if (*n) buf[i] ^= (uint8_t)(1u << (lj_fuzz_next(s) & 7)); break;
    case 1: if (*n) buf[i] = (uint8_t)lj_fuzz_next(s); break;
    case 2: if (*n) buf[i] = edge[lj_fuzz_next(s) % sizeof edge]; break;
    case 3: *n = *n ? lj_fuzz_next(s) % (*n + 1) : 0; break;
    case 4: k = lj_fuzz_next(s) % 16 + 1; while (k-- && *n < cap) buf[(*n)++] = (uint8_t)lj_fuzz_next(s); break;
    default: if (*n > 1) { size_t j = lj_fuzz_next(s) % *n, len = lj_fuzz_next(s) % (*n - (i > j ? i : j)) + 1; memmove(buf + i, buf + j, len < *n - i ? len : *n - i); } break;
    }
}

/* ME-186: `lj4evr_tests <fuzz row> --seed <dir>` writes the row's proven seed into <dir>/<name>, the libFuzzer corpus the
 * CI fuzz step starts from (docs/ci.md), so the accepted paths are fuzzed from the first second. 1 = written (the row
 * returns), 0 = no --seed argument, -1 = cannot write. */
static inline int lj_fuzz_seed(int argc, char **argv, const char *name, const void *p, size_t n)
{
    char path[512];
    FILE *f;
    if (argc < 3 || strcmp(argv[1], "--seed") != 0) return 0;
    lj_fs_mkdir(argv[2]);
    if (snprintf(path, sizeof path, "%s/%s", argv[2], name) >= (int)sizeof path) return -1;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)   /* fopen: the test tree's own scratch file */
#endif
    f = fopen(path, "wb");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (!f) return -1;
    if (fwrite(p, 1, n, f) != n) { fclose(f); return -1; }
    return fclose(f) == 0 ? 1 : -1;
}

#endif /* LJ_TESTS_FUZZ_H */
