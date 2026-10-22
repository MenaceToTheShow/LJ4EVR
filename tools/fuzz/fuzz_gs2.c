/* fuzz_gs2.c - libFuzzer target over lj_gs2_read (LORD_C-17; CMake -DLJ_FUZZ=ON, clang). Any accepted input must
 * write back byte-exact (the raw-record rule), so the target checks the round trip as well as memory safety. */
#include <stdlib.h>
#include <string.h>
#include "fixlib/lj_gs2.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static uint8_t out[LJ_GS2_REC_SIZE];
    lj_profile p;
    char err[128];
    memset(&p, 0, sizeof p);
    if (lj_gs2_read(data, size, &p, err, sizeof err) == 0) {
        if (lj_gs2_write(&p, out) != 0 || memcmp(out, data, LJ_GS2_REC_SIZE) != 0) abort();
        lj_gs2_free(&p);
    }
    return 0;
}
