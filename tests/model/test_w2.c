/* test_w2.c - `lj4evr_tests test_model_w2_range` / `test_model_w2_cuepages`: the W2 model contract.
 * test_model_w2_range: lj_kv_range (ME-77) rejects level=356, cue_time_pct=65636, spt=257, link=257,
 * addr=65537 instead of aliasing, returns the default when absent, flags a non-number.
 * test_model_w2_cuepages: the CuePageInfo.Dat layer that exists (lj_cue_pages_load/save) round-trips
 * two pages; the lj_cuepages operations are the model track's rows (stubs return -1 today, recorded). */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include "model/lj_cuepage.h"
#include "model/lj_kv.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_model_w2_range(int argc, char **argv);
int lj_test_model_w2_range(int argc, char **argv)
{
    int bad;
    (void)argc; (void)argv;
    CHECK(lj_kv_range("slot 1 level=356", "level", 100, 0, 100, NULL) == 100);                 /* NULL bad is allowed */
    bad = 0; CHECK(lj_kv_range("slot 1 level=356", "level", 100, 0, 100, &bad) == 100 && bad == 1);
    bad = 0; CHECK(lj_kv_range("cuetime=65636", "cuetime", 100, 1, 999, &bad) == 100 && bad == 1);
    bad = 0; CHECK(lj_kv_range("spt=257 atpt=1", "spt", 1, 1, 255, &bad) == 1 && bad == 1);
    bad = 0; CHECK(lj_kv_range("link=257", "link", 0, 1, 4, &bad) == 0 && bad == 1);
    bad = 0; CHECK(lj_kv_range("addr=65537", "addr", 1, 1, 512, &bad) == 1 && bad == 1);
    bad = 0; CHECK(lj_kv_range("addr=512", "addr", 1, 1, 512, &bad) == 512 && bad == 0);
    bad = 0; CHECK(lj_kv_range("addr=-1", "addr", 1, 1, 512, &bad) == 1 && bad == 1);
    bad = 0; CHECK(lj_kv_range("name=x", "addr", 7, 1, 512, &bad) == 7 && bad == 0);           /* absent = default, not bad */
    bad = 0; CHECK(lj_kv_range("addr=12x", "addr", 7, 1, 512, &bad) == 7 && bad == 1);         /* not a number */
    printf("test_model_w2_range: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}

int lj_test_model_w2_cuepages(int argc, char **argv);
int lj_test_model_w2_cuepages(int argc, char **argv)
{
    lj_cue_page in[2], out[LJ_CUE_PAGES]; int n;
    const char *path = "test_w2_cuepages.dat";
    (void)argc; (void)argv;
    memset(in, 0, sizeof in);
    strcpy(in[0].name, "Page 1"); in[0].n = 4; in[0].cue[0] = 1; in[0].cue[1] = 5; in[0].cue[2] = 5; in[0].cue[3] = 9;   /* the same cue twice [03 s5.8] */
    strcpy(in[1].name, "Specials"); in[1].n = 0;
    CHECK(lj_cue_pages_save(path, in, 2) == 0);
    n = lj_cue_pages_load(path, out, LJ_CUE_PAGES);
    CHECK(n == 2);
    CHECK(n == 2 && strcmp(out[0].name, "Page 1") == 0 && out[0].n == 4 && out[0].cue[1] == 5 && out[0].cue[2] == 5 && out[0].cue[3] == 9);
    CHECK(n == 2 && strcmp(out[1].name, "Specials") == 0 && out[1].n == 0);
    remove(path);
    printf("test_model_w2_cuepages: %d failure(s) (lj_cuepages_* operations: W2 model track)\n", fails);
    return fails ? 1 : 0;
}
