/* test_engine_stub.c - the W0 engine stub: command array fills at 256 and drops
 * with a count (design 2.3 rule 1), the frame is the documented table pattern,
 * pending_* fields clear once handed out; and "dump-frames [path]": 400 ticks to
 * frames.bin (merged[2048] + out[4][512] + tick_no as 8 LE bytes per tick) for
 * the CI determinism cmp job (design 9.3). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf/sprintf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "engine/lj_trig_gen.h"
#include "core/lj_log.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int s_drop_lines;
static void lj_count_drops(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "command array full")) s_drop_lines++;
}

int lj_test_engine_stub(int argc, char **argv);
int lj_test_engine_stub(int argc, char **argv)
{
    lj_engine *e = lj_engine_create();
    lj_engine_status st;
    lj_cmd cmd;
    const lj_frame *f;
    int i, ok = 0, ch;
    (void)argc; (void)argv;
    CHECK(e != NULL);
    if (!e) return 1;
    lj_engine_bind(e, NULL);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_GO;
    lj_log_set_sink(lj_count_drops, NULL);
    for (i = 0; i < 300; i++) ok += lj_engine_cmd(e, &cmd) == 0;
    lj_log_set_sink(NULL, NULL);
    CHECK(ok == LJ_ENGINE_CMD_CAP && s_drop_lines == 300 - LJ_ENGINE_CMD_CAP);   /* 44 logged drops, design 2.3 */
    lj_engine_view(e, &st);
    CHECK(st.drops == 300 - LJ_ENGINE_CMD_CAP);
    lj_engine_tick(e, 0);                                        /* drains */
    CHECK(lj_engine_cmd(e, &cmd) == 0);
    f = lj_engine_frame(e);
    CHECK(f->tick_no == 1);
    for (i = 1; i < 3; i++) lj_engine_tick(e, (uint64_t)i * LJ_TICK_US_DEFAULT);
    CHECK(f->tick_no == 3);
    for (ch = 0, ok = 1; ch < LJ_CHANS; ch++) {
        uint8_t want = (uint8_t)((lj_sin4096[(2 * 7 + ch * 3) & 4095] >> 8) + 128);
        if (f->merged[ch] != want || f->out[ch / LJ_LINK_CH][ch % LJ_LINK_CH] != want || f->layer_of[ch]) ok = 0;
    }
    CHECK(ok);
    lj_engine_destroy(e);
    lj_engine_destroy(NULL);
    printf("test_engine_stub: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}

int lj_test_dump_frames(int argc, char **argv);
int lj_test_dump_frames(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "frames.bin";
    lj_engine *e = lj_engine_create();
    const lj_frame *f;
    FILE *out;
    int i, k, ok = 1;
    if (!e) return 1;
    out = fopen(path, "wb");
    if (!out) { printf("dump-frames: cannot write %s\n", path); lj_engine_destroy(e); return 1; }
    f = lj_engine_frame(e);
    for (i = 0; i < 400 && ok; i++) {
        uint8_t tn[8];
        lj_engine_tick(e, (uint64_t)i * LJ_TICK_US_DEFAULT);
        for (k = 0; k < 8; k++) tn[k] = (uint8_t)(f->tick_no >> (8 * k));
        ok = fwrite(f->merged, 1, sizeof f->merged, out) == sizeof f->merged
          && fwrite(f->out, 1, sizeof f->out, out) == sizeof f->out
          && fwrite(tn, 1, 8, out) == 8;
    }
    ok = fclose(out) == 0 && ok;
    lj_engine_destroy(e);
    printf("dump-frames: %d tick(s) -> %s%s\n", i, path, ok ? "" : " (write failed)");
    return ok ? 0 : 1;
}
