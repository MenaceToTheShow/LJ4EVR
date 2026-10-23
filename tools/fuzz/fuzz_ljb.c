/* fuzz_ljb.c - libFuzzer target over lj_ljb_open / lj_ljb_extract, the .LJB container boundary (W5-LJB-1, W5-X-HOSTILE;
 * CMake -DLJ_FUZZ=ON, clang). As fuzz_udf (ME-114) the trailer CRC is rewritten with mz_crc32 of the body first, so the
 * index walk, the path rules and the inflate cap are what the fuzzer explores. Seed: a container from test_ljb's
 * ljb_pack (synthetic, ours).
 * Built twice (LORD_C-7). fuzz_ljb mutates the container bytes. fuzz_ljb_index (-DLJ_FUZZ_LJB_INDEX) reads a container
 * whose index is stored raw (seed: `lj4evr_tests fuzz_ljb --seed-index <dir>`), deflates that index again and rewrites
 * the trailer size, so the mutations land on the inflated 532-byte records instead of on zlib bytes. */
#include <stdlib.h>
#include <string.h>
#include "model/lj_ljb.h"
#include "miniz.h"

#ifdef LJ_FUZZ_LJB_INDEX
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
#endif
static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static uint8_t buf[2 * 65536 + 1024];
    lj_ljb b;
    uint8_t *x;
    size_t xn;
    uint32_t i;
    char err[128];
    if (size > 65536) return 0;
#ifdef LJ_FUZZ_LJB_INDEX
    {
        uint32_t ioff, isz;
        mz_ulong zl;
        if (size < LJ_LJB_TRAILER) return 0;
        ioff = rd32(data + size - LJ_LJB_TRAILER + 16); isz = rd32(data + size - LJ_LJB_TRAILER + 12);
        if ((uint64_t)ioff + isz > size - LJ_LJB_TRAILER) return 0;
        memcpy(buf, data, ioff);
        zl = (mz_ulong)(sizeof buf - ioff - LJ_LJB_TRAILER);
        if (mz_compress2(buf + ioff, &zl, data + ioff, isz, 1) != MZ_OK) return 0;
        memcpy(buf + ioff + zl, data + size - LJ_LJB_TRAILER, LJ_LJB_TRAILER);
        le32(buf + ioff + zl + 12, (uint32_t)zl);
        size = ioff + zl + LJ_LJB_TRAILER;
    }
#else
    memcpy(buf, data, size);
#endif
    if (size >= LJ_LJB_TRAILER) le32(buf + size - LJ_LJB_TRAILER, (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, size - LJ_LJB_TRAILER));
    if (lj_ljb_open(&b, buf, size, err, sizeof err) != 0) return 0;
    for (i = 0; i < b.n; i++) if (lj_ljb_extract(&b, i, &x, &xn, err, sizeof err) == 0) free(x);
    lj_ljb_free(&b);
    return 0;
}
