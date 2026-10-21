/* test_scene.c - `lj4evr_tests test_model_scene`: the F29 scene rules of
 * lj_scene.h: a new sequence has one all-off scene, Add appends a copy of the
 * current scene, Insert goes before the current scene, Delete of a range, delete
 * all leaves one all-off scene, the 999 cap. */
#include <stdio.h>
#include <string.h>
#include "model/lj_scene.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_model_scene(int argc, char **argv);
int lj_test_model_scene(int argc, char **argv)
{
    lj_sequence seq = { 0 };
    lj_scene blank;
    int i;
    (void)argc; (void)argv;

    lj_scene_init(&blank);
    lj_sequence_new(&seq);
    CHECK(seq.n == 1 && seq.number == 0 && seq.scenes && !lj_scene_is_programmed(&seq.scenes[0]));
    CHECK(seq.scenes[0].scene_ms == 1000 && seq.scenes[0].fade_ms == 50);
    seq.scenes[0].val[5] = 200; seq.scenes[0].state[5] = LJ_FADE; seq.scenes[0].scene_ms = 460;
    CHECK(lj_scene_is_programmed(&seq.scenes[0]));

    CHECK(lj_sequence_add(&seq, 0) == 1 && seq.n == 2 && memcmp(&seq.scenes[1], &seq.scenes[0], sizeof blank) == 0);   /* Add copies the current scene */
    seq.scenes[1].val[5] = 10;
    CHECK(lj_sequence_insert(&seq, 1, 2, 1) == 1 && seq.n == 4);        /* two copies of scene 1 inserted before it */
    CHECK(seq.scenes[1].val[5] == 10 && seq.scenes[2].val[5] == 10 && seq.scenes[3].val[5] == 10 && seq.scenes[0].val[5] == 200);
    CHECK(lj_sequence_insert(&seq, 0, 1, 0) == 0 && seq.n == 5 && memcmp(&seq.scenes[0], &blank, sizeof blank) == 0 && seq.scenes[1].val[5] == 200);   /* a blank one before scene 0 */
    CHECK(lj_sequence_insert(&seq, 5, 1, 0) == -1);                     /* before must be an existing scene */
    CHECK(lj_sequence_delete(&seq, 1, 3) == 0 && seq.n == 2 && seq.scenes[1].val[5] == 10);
    CHECK(lj_sequence_delete(&seq, 1, 5) == -1 && lj_sequence_delete(&seq, 1, 0) == -1);
    CHECK(lj_sequence_delete(&seq, 0, 1) == 0 && seq.n == 1 && memcmp(&seq.scenes[0], &blank, sizeof blank) == 0);   /* delete all = one all-off scene */

    CHECK(lj_sequence_alloc(&seq, LJ_SCENES_MAX) == 0 && seq.n == 999);
    CHECK(lj_sequence_add(&seq, 0) == -1 && lj_sequence_insert(&seq, 0, 1, 0) == -1 && seq.n == 999);   /* the 999 cap */
    CHECK(lj_sequence_alloc(&seq, 1000) == -1 && lj_sequence_alloc(&seq, 0) == -1 && seq.n == 999);
    for (i = 0; i < 999; i++) CHECK(seq.scenes[i].scene_ms == 1000);
    lj_sequence_free_scenes(&seq);
    CHECK(seq.n == 0 && seq.scenes == NULL);

    printf("test_model_scene: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
