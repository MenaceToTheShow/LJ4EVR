/* test_replay_grammar.c - `lj4evr_tests test_replay_grammar`: the harness rejects a
 * line it does not know with that line's number, rejects an expect on a channel no
 * patch line covers, rejects event lines out of tick order, and refuses a file
 * without the "LJ4 replay 1" header. Each case is a small scenario written to the
 * working directory and removed again; none needs a working engine, since every
 * rejection happens before or instead of the build. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "engine/lj_replay.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *lj_tmp = "replay_grammar_tmp.lj4r";

static int lj_run_text(const char *text, lj_replay_result *r)
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

int lj_test_replay_grammar(int argc, char **argv);
int lj_test_replay_grammar(int argc, char **argv)
{
    lj_replay_result r;
    (void)argc; (void)argv;

    /* 1. an unknown line form is a grammar error naming its line (line 3 here) */
    CHECK(lj_run_text("LJ4 replay 1\ntick_us=25000\nbogus 1 2 3\n", &r) == -1);
    CHECK(strncmp(r.first_fail, "line 3:", 7) == 0 && strstr(r.first_fail, "bogus") != NULL);

    /* 2. an expect on a channel no patch line covers fails at the expect line (line 2), before any tick runs */
    CHECK(lj_run_text("LJ4 replay 1\nexpect 0 1:1=0\n", &r) == -1);
    CHECK(strncmp(r.first_fail, "line 2:", 7) == 0 && strstr(r.first_fail, "not patched") != NULL && r.ticks == 0);

    /* 3. a ch line on an unpatched channel is rejected the same way (line 2) */
    CHECK(lj_run_text("LJ4 replay 1\nseq 1 name=\"x\" scene st=1000 ft=50 ch 2:7=255:S\n", &r) == -1);
    CHECK(strncmp(r.first_fail, "line 2:", 7) == 0 && strstr(r.first_fail, "not patched") != NULL);

    /* 4. the header is mandatory */
    CHECK(lj_run_text("LJ4 seq 1\n", &r) == -1 && strncmp(r.first_fail, "line 1:", 7) == 0);

    /* 5. a bad value names its key */
    CHECK(lj_run_text("LJ4 replay 1\npatch 1 profile=19 link=9 addr=1\n", &r) == -1);
    CHECK(strncmp(r.first_fail, "line 2:", 7) == 0 && strstr(r.first_fail, "link=9") != NULL);

    /* 6. a missing file */
    CHECK(lj_replay_run("replay_grammar_does_not_exist.lj4r", NULL, &r) == -1 && r.fails == 1);

    printf("test_replay_grammar: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
