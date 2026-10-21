/* test_replay.c - `lj4evr_tests replay [file...]`: runs every scenario named on the
 * command line through lj_replay_run and fails if any expect failed. With no
 * files it prints "replay: 0 scenario(s)" and passes, so the CTest row
 * replay_all (every tests/engine/replay/*.lj4r, globbed by CMake) is green
 * before the first scenario lands. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "engine/lj_replay.h"

int lj_test_replay(int argc, char **argv);
int lj_test_replay(int argc, char **argv)
{
    int i, fails = 0;
    lj_replay_result r;
    for (i = 1; i < argc; i++)
        if (lj_replay_run(argv[i], NULL, &r) != 0) fails++;
    printf("replay: %d scenario(s), %d failed\n", argc - 1, fails);
    return fails ? 1 : 0;
}

/* `lj4evr_tests --dump-frames <out.bin> <file.lj4r>...`: every scenario's frames
 * concatenated for the CI determinism cmp job. Per scenario a 64-byte header
 * ("LJ4R" + the scenario name, NUL-padded) precedes its frames (merged[2048] +
 * out[4][512] + tick_no as 8 LE bytes per tick, lj_replay.h), so a divergence
 * in the third OS's dump lands inside a named block. */
int lj_test_replay_dump(int argc, char **argv);
int lj_test_replay_dump(int argc, char **argv)
{
    FILE *out;
    int i, fails = 0, ok = 1;
    lj_replay_result r;
    if (argc < 2) { printf("usage: --dump-frames <out.bin> <file.lj4r>...\n"); return 2; }
    out = fopen(argv[1], "wb");
    if (!out) { printf("--dump-frames: cannot write %s\n", argv[1]); return 1; }
    for (i = 2; i < argc; i++) {
        char hdr[64];
        const char *s = argv[i], *e;
        for (e = s; *e; e++) if (*e == '/' || *e == '\\') s = e + 1;
        memset(hdr, 0, sizeof hdr);
        memcpy(hdr, "LJ4R", 4);
        snprintf(hdr + 4, sizeof hdr - 4, "%s", s);
        ok = fwrite(hdr, 1, sizeof hdr, out) == sizeof hdr && ok;
        if (lj_replay_run(argv[i], out, &r) != 0) fails++;
    }
    ok = fclose(out) == 0 && ok;
    printf("--dump-frames: %d scenario(s), %d failed -> %s%s\n", argc - 2, fails, argv[1], ok ? "" : " (write failed)");
    return (fails || !ok) ? 1 : 0;
}
