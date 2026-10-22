/* fuzz_udf.c - libFuzzer target over lj_udf_parse, the .Udf / .Udo container boundary (LORD_C-17; CMake -DLJ_FUZZ=ON,
 * clang). ME-114: random input never matched the footer CRC (cov 175), so the target rewrites the footer's 4 CRC
 * bytes with mz_crc32 of the body before parsing; the inflate and record paths are then what the fuzzer explores. CI
 * seeds the corpus with test_uiscript_w3's lj_w3_synthetic.Udf (an lj_udf_export file) and runs -max_len=32768. */
#include <string.h>
#include "fixlib/lj_gs2.h"
#include "miniz.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static uint8_t buf[32768];
    lj_profile p;
    char err[128];
    uint32_t crc;
    if (size > sizeof buf) return 0;
    memcpy(buf, data, size);
    if (size >= LJ_UDF_FOOTER) {
        crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, size - LJ_UDF_FOOTER);
        buf[size - LJ_UDF_FOOTER] = (uint8_t)crc; buf[size - LJ_UDF_FOOTER + 1] = (uint8_t)(crc >> 8);
        buf[size - LJ_UDF_FOOTER + 2] = (uint8_t)(crc >> 16); buf[size - LJ_UDF_FOOTER + 3] = (uint8_t)(crc >> 24);
    }
    memset(&p, 0, sizeof p);
    if (lj_udf_parse(buf, size, &p, err, sizeof err) == 0) lj_gs2_free(&p);
    return 0;
}
