/* fuzz_remote.c - libFuzzer target over the remote protocol (io/lj_remote.h, W5-REM-1, W5-X-HOSTILE; CMake -DLJ_FUZZ=ON,
 * clang). The input is a byte stream; byte 0 spaces the arrival times (0..255 ms per chunk of 7 bytes, around the
 * 0.2 s frame window); every decoded frame runs through lj_remote_exec with no function bound and no state query. */
#include <string.h>
#include "io/lj_remote.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    lj_remote_parser p;
    lj_remote_msg m[4];
    uint64_t now = 1000000;
    size_t off, k;
    int got, i;
    if (size < 1) return 0;
    lj_remote_parser_init(&p);
    for (off = 1; off < size; off += k) {
        k = size - off < 7 ? size - off : 7;
        now += (uint64_t)data[0] * 1000u;
        got = lj_remote_feed(&p, data + off, k, now, m, 4);
        for (i = 0; i < got; i++) lj_remote_exec(&m[i]);
    }
    lj_remote_inject(data, size, now);
    return 0;
}
