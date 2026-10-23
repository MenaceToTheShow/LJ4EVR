/* fuzz_ljb.c - libFuzzer target over lj_ljb_open / lj_ljb_extract, the .LJB container boundary (W5-LJB-1, W5-X-HOSTILE;
 * CMake -DLJ_FUZZ=ON, clang). As fuzz_udf (ME-114) the trailer CRC is rewritten with mz_crc32 of the body first, so the
 * index walk, the path rules and the inflate cap are what the fuzzer explores. Seed: a container from test_ljb's
 * ljb_pack (synthetic, ours). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_ljb.h"
#include "miniz.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static uint8_t buf[65536];
    lj_ljb b;
    uint8_t *x;
    size_t xn;
    uint32_t crc, i;
    char err[128];
    if (size > sizeof buf) return 0;
    memcpy(buf, data, size);
    if (size >= LJ_LJB_TRAILER) {
        crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, size - LJ_LJB_TRAILER);
        buf[size - LJ_LJB_TRAILER] = (uint8_t)crc; buf[size - LJ_LJB_TRAILER + 1] = (uint8_t)(crc >> 8);
        buf[size - LJ_LJB_TRAILER + 2] = (uint8_t)(crc >> 16); buf[size - LJ_LJB_TRAILER + 3] = (uint8_t)(crc >> 24);
    }
    if (lj_ljb_open(&b, buf, size, err, sizeof err) != 0) return 0;
    for (i = 0; i < b.n; i++) if (lj_ljb_extract(&b, i, &x, &xn, err, sizeof err) == 0) free(x);
    lj_ljb_free(&b);
    return 0;
}
