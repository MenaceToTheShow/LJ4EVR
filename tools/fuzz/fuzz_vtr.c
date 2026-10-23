/* fuzz_vtr.c - libFuzzer target over the .vtr reader (W5-X-HOSTILE, W5-DMXIN-6; CMake -DLJ_FUZZ=ON, clang): an accepted
 * table carries NUL-terminated strings and a file name that never holds a path separator. */
#include <stdlib.h>
#include <string.h>
#include "io/lj_dmxin.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    lj_vtr t;
    char err[128], name[256];
    if (lj_vtr_parse((const char *)data, size, &t, err, sizeof err) != 0) return 0;
    if (memchr(t.id, 0, sizeof t.id) == NULL || memchr(t.name, 0, sizeof t.name) == NULL || memchr(t.type, 0, sizeof t.type) == NULL || memchr(t.purpose, 0, sizeof t.purpose) == NULL) abort();
    if (lj_vtr_file_name(name, sizeof name, &t) == 0 && (strchr(name, '/') || strchr(name, '\\') || strchr(name, ':'))) abort();
    return 0;
}
