/* test_replay_w2.c - `lj4evr_tests test_replay_w2`: the W2 replay line forms (docs/replay-grammar.md
 * "W2 replay-track lines"). A malformed `seed`, `ack_bg`, `load_cue ... nobg`, runtime `at k gpatch` /
 * `at k default` or W2 `at` form is rejected with its line number, and a bad runtime edit runs no tick;
 * a good runtime edit lands before its tick and never leaks into the ticks before it. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "engine/lj_replay.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *lj_tmp = "replay_w2_tmp.lj4r";

static int lj_run(const char *text, lj_replay_result *r)
{
    int rc;
    FILE *f = fopen(lj_tmp, "wb");
    if (!f) { printf("cannot write %s\n", lj_tmp); return -2; }
    fputs(text, f);
    fclose(f);
    rc = lj_replay_run(lj_tmp, NULL, r);
    remove(lj_tmp);
    return rc;
}

/* rc -1, first failure on `line`, naming `word` */
static int lj_rejected(const char *text, int line, const char *word, lj_replay_result *r)
{
    char want[16];
    snprintf(want, sizeof want, "line %d:", line);
    return lj_run(text, r) == -1 && strncmp(r->first_fail, want, strlen(want)) == 0 && strstr(r->first_fail, word) != NULL;
}

#define LJ_DIMMER "LJ4 replay 1\npatch 1 profile=19 link=1 addr=1\n"   /* lines 1-2 */

int lj_test_replay_w2(int argc, char **argv);
int lj_test_replay_w2(int argc, char **argv)
{
    lj_replay_result r;
    (void)argc; (void)argv;

    /* seed: the scenario's LJ_RNG_SEED must be the engine's; a non-number is a bad value */
    CHECK(lj_rejected("LJ4 replay 1\nseed 5\n", 2, "LJ_RNG_SEED", &r));
    CHECK(lj_rejected("LJ4 replay 1\nseed 0xZZ\n", 2, "seed=0xZZ", &r));
    CHECK(lj_rejected("LJ4 replay 1\nseed\n", 2, "seed", &r));
    CHECK(lj_run("LJ4 replay 1\nseed 0x9E3779B9\n", &r) == 0);

    /* ack_bg with nothing pending, with an argument */
    CHECK(lj_rejected(LJ_DIMMER "at 0 ack_bg\n", 3, "no pending bg", &r));
    CHECK(lj_rejected(LJ_DIMMER "at 0 ack_bg 1\n", 3, "ack_bg", &r));

    /* load_cue takes only [edit] [nobg] */
    CHECK(lj_rejected(LJ_DIMMER "seq 1 scene st=1000 ft=50 ch 1:1=9:S\ncue 1 slot=1 seq=1\nat 0 load_cue 1 nobgx\n", 5, "load_cue", &r));

    /* runtime edits: bad channel / value / key rejected on their line before any tick runs */
    CHECK(lj_rejected(LJ_DIMMER "at 4 gpatch 9:1 inv=1\n", 3, "gpatch", &r) && r.ticks == 0);
    CHECK(lj_rejected(LJ_DIMMER "at 4 gpatch 1:1 bogus=1\n", 3, "bogus", &r) && r.ticks == 0);
    CHECK(lj_rejected(LJ_DIMMER "at 4 default 1:1=300\n", 3, "default", &r) && r.ticks == 0);
    CHECK(lj_rejected(LJ_DIMMER "at 4 default\n", 3, "no channels", &r) && r.ticks == 0);
    CHECK(lj_rejected(LJ_DIMMER "at 4 default 1:1=7 at_us=1\n", 3, "at_us", &r) && r.ticks == 0);

    /* a W2 `at` form with a bad word names its line */
    CHECK(lj_rejected(LJ_DIMMER "at 0 transparent maybe\n", 3, "transparent", &r));

    /* a good runtime default edit lands before tick 3 and not before (DEFAULT_OUT hands the table over) */
    CHECK(lj_run(LJ_DIMMER "at 3 default 1:1=77\nat 3 default_out\nexpect 2 1:1=0\nexpect 3 1:1=77\n", &r) == 0 && r.expects == 2);

    /* a good runtime gpatch edit (S6 invert) the same way */
    CHECK(lj_run(LJ_DIMMER "expect 1 1:1=0\nat 2 gpatch 1:1 inv=1\nat 2 global_patch\nexpect 2 1:1=255\n", &r) == 0 && r.expects == 2);

    printf("test_replay_w2: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
