/* test_remote.c - the remote track's rows (docs/w5-contracts.md seam 5). The contract seeds the fuzz row (W5-X-HOSTILE):
 *   fuzz_remote  the note 05 s10.1 example (cue 130 = 129, 1, 2) plus a blackout frame must parse to two messages, then
 *                LJ_FUZZ_ITERS mutated byte streams with random arrival times through lj_remote_feed
 * It prints PENDING remote and fails until the track lands the parser (DISABLED in CMakeLists.txt until then). The track
 * appends its W5-REM-1..4 rows to this file. */
#include <stdio.h>
#include <string.h>
#include "io/lj_remote.h"
#include "lj_fuzz.h"

int lj_test_fuzz_remote(int argc, char **argv)
{
    static const uint8_t seed[] = { 129, 1, 2, 132, 2, 1 };
    uint8_t buf[256];
    lj_remote_parser p;
    lj_remote_msg m[8];
    uint64_t now = 1000000;
    uint32_t s = 0x232u;
    size_t n;
    int i, k, got, bad = 0;
    (void)argc; (void)argv;
    lj_remote_parser_init(&p);
    got = lj_remote_feed(&p, seed, sizeof seed, now, m, 8);
    if (got != 2 || m[0].cmd != LJ_REM_CUE || m[0].value != 130 || m[1].cmd != LJ_REM_MISC || m[1].d1 != 2 || m[1].d2 != 1) {
        printf("PENDING remote: lj_remote_feed does not decode the 05 s10.1 example yet\n");
        return 1;
    }
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, seed, sizeof seed); n = sizeof seed;
        for (k = (int)(lj_fuzz_next(&s) % 12); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        now += lj_fuzz_next(&s) % 300000u;                       /* around the 0.2 s frame window */
        got = lj_remote_feed(&p, buf, n, now, m, 8);
        if (got < 0 || got > 8) { if (!bad++) printf("FAIL iteration %d: %d messages for max 8\n", i, got); continue; }
        for (k = 0; k < got; k++)
            if (m[k].cmd < 128 || m[k].d1 > 127 || m[k].d2 > 127 || m[k].value != m[k].d1 * 128 + m[k].d2) { if (!bad++) printf("FAIL iteration %d: malformed message\n", i); }
    }
    printf("fuzz_remote: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
