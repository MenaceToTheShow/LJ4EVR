/* test_table.c - `lj4evr_tests test_model_table`: lj_table (sorted number -> record
 * map, design 4) and the record words of lj_words.h: put/get/replace/remove,
 * ascending iteration across a growth boundary, free with and without a record
 * destructor, word round trips and the unknown-word -1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_table.h"
#include "model/lj_model.h"
#include "model/lj_words.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int freed;
static void lj_count_free(void *p) { freed++; free(p); }

int lj_test_model_table(int argc, char **argv);
int lj_test_model_table(int argc, char **argv)
{
    lj_table t;
    int i, a = 1, b = 2, c = 3;
    uint32_t num, prev;
    (void)argc; (void)argv;

    lj_table_init(&t);
    CHECK(lj_table_count(&t) == 0 && lj_table_get(&t, 1) == NULL);
    CHECK(lj_table_put(&t, 12, &a) == 0 && lj_table_put(&t, 3, &b) == 0 && lj_table_put(&t, 7, &c) == 0);
    CHECK(lj_table_put(&t, 0, &a) == -1 && lj_table_put(&t, 5, NULL) == -1);          /* number 0 and NULL rejected */
    CHECK(lj_table_count(&t) == 3 && lj_table_get(&t, 12) == &a && lj_table_get(&t, 3) == &b && lj_table_get(&t, 7) == &c);
    CHECK(lj_table_at(&t, 0, &num) == &b && num == 3);
    CHECK(lj_table_at(&t, 2, &num) == &a && num == 12);
    CHECK(lj_table_at(&t, 3, &num) == NULL);
    CHECK(lj_table_put(&t, 7, &a) == 0 && lj_table_count(&t) == 3 && lj_table_get(&t, 7) == &a);   /* replace keeps the count */
    CHECK(lj_table_remove(&t, 7) == &a && lj_table_count(&t) == 2 && lj_table_get(&t, 7) == NULL);
    CHECK(lj_table_remove(&t, 7) == NULL);
    lj_table_free(&t, NULL);                                                            /* records are stack ints: not freed */
    CHECK(t.n == 0 && t.e == NULL);

    for (i = 200; i >= 1; i--) CHECK(lj_table_put(&t, (uint32_t)i, malloc(4)) == 0);   /* crosses the 64/128 growth boundaries, descending insert */
    CHECK(lj_table_count(&t) == 200);
    for (i = 0, prev = 0; i < 200; i++) { CHECK(lj_table_at(&t, i, &num) != NULL && num == prev + 1); prev = num; }
    lj_table_free(&t, lj_count_free);
    CHECK(freed == 200 && t.n == 0);

    CHECK(strcmp(lj_word(lj_trig_words, LJ_TRIG_AUDIO), "audio") == 0);
    CHECK(strcmp(lj_word(lj_dir_words, 99), "fwd") == 0);                               /* out of range -> table[0] */
    CHECK(lj_word_index(lj_end_words, "LINK") == LJ_END_LINK);
    CHECK(lj_word_index(lj_replace_words, "solo_all") == LJ_REP_SOLO_ALL);
    CHECK(lj_word_index(lj_fstate_words, "F") == LJ_FADE && lj_word_index(lj_fstate_words, "x") == -1);
    CHECK(lj_word_index(lj_in_mode_words, NULL) == -1);

    printf("test_model_table: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
