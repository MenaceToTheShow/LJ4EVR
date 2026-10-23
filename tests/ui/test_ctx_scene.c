/* test_ctx_scene.c - ME-129: one current-scene getter. lj_ctx_scene() returns NULL past the end and never clamps;
 * the clamp happens once, where the sequence shrinks (lj_seq_form_delete_scene). Generic DMX Control, the Movement
 * Macro editor, the fixture controls, User Palettes and the Sequence window all read lj_ctx_scene, so after a
 * shortened sequence they agree on the scene they write (test_layering counts 0 private getters). */
#include <stdio.h>
#include "desktop/lj_desktop.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "windows/lj_seq_form.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_ctx_scene(int argc, char **argv);
int lj_test_ctx_scene(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *keep = c->show, *show = lj_show_new();
    lj_sequence *s = lj_sequence_new_rec();
    uint16_t keep_scene = c->current_scene;
    (void)argc; (void)argv;
    c->show = show;
    show->current_seq = s;
    CHECK(lj_sequence_add(s, 0) == 1 && lj_sequence_add(s, 1) == 2 && s->n == 3);
    c->current_scene = 2;
    CHECK(lj_ctx_scene() == &s->scenes[2]);
    CHECK(lj_seq_form_delete_scene() == 0 && s->n == 2);          /* the shrink clamps once: current scene 2 -> 1 */
    CHECK(c->current_scene == 1 && lj_ctx_scene() == &s->scenes[1]);
    s->n = 1;                                                      /* a shrink that forgot to clamp: every reader gets NULL, none writes */
    CHECK(lj_ctx_scene() == NULL && c->current_scene == 1);        /* and the getter does not move the operator */
    s->n = 2;
    c->show = NULL;
    CHECK(lj_ctx_scene() == NULL);
    c->show = keep; c->current_scene = keep_scene;
    show->current_seq = NULL;
    lj_sequence_free(s);
    lj_show_free(show);
    printf("ctx_scene: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
