/* fuzz_udf.c - libFuzzer target over lj_udf_parse, the .Udf / .Udo container boundary (LORD_C-17; CMake -DLJ_FUZZ=ON,
 * clang). Most mutations fail the footer CRC; seed the corpus with lj_udf_export output so the inflate path is reached. */
#include <string.h>
#include "fixlib/lj_gs2.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    lj_profile p;
    char err[128];
    memset(&p, 0, sizeof p);
    if (lj_udf_parse(data, size, &p, err, sizeof err) == 0) lj_gs2_free(&p);
    return 0;
}
