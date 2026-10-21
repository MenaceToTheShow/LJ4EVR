/* lj_replay.h - the replay test harness (design 3.7, docs/replay-grammar.md):
 * builds a show from a .lj4r text file the way the UI thread would (records
 * loaded and pinned, commands posted at the named ticks), drives
 * lj_engine_tick with synthetic time and asserts frames. Used by
 * `lj4evr_tests replay <file>...` and by `dump-frames` (every scenario's frames
 * appended to frames.bin for the cross-OS cmp job). Test code: no lj_ layering
 * rules apply here, but it stays SDL-free and float-free like the engine. */
#ifndef LJ_TESTS_ENGINE_REPLAY_H
#define LJ_TESTS_ENGINE_REPLAY_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    int ticks;            /* ticks run */
    int expects, fails;   /* expect lines seen / failed */
    uint32_t tick_us;
    uint32_t max_tick_us; /* longest lj_engine_tick measured (perf line) */
    char first_fail[256]; /* "line 12: 1:1 expected 128 got 127" */
} lj_replay_result;

/* Runs one scenario. frames_out != NULL: every frame appended in the dump-frames
 * layout (merged[2048] + out[4][512] + tick_no as 8 LE bytes per tick). Prints
 * "FAIL <path>:<line> ..." per failed expect and "replay <name>: N ticks, E expects, F fails".
 * Returns 0 when every expect held, -1 on any failure or a grammar error. */
int lj_replay_run(const char *path, FILE *frames_out, lj_replay_result *out);

#endif /* LJ_TESTS_ENGINE_REPLAY_H */
