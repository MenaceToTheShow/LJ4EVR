/* lj_scene.c - scene ops of lj_scene.h (design 4 last paragraph, [F29], 03 s4.3). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_scene.h"

void lj_scene_init(lj_scene *s)
{
    memset(s, 0, sizeof *s);
    s->scene_ms = LJ_SCENE_TIME_DEFAULT_MS;
    s->fade_ms = LJ_FADE_TIME_DEFAULT_MS;
}

void lj_scene_copy(lj_scene *dst, const lj_scene *src) { memcpy(dst, src, sizeof *dst); }

int lj_scene_is_programmed(const lj_scene *s)
{
    int i;
    for (i = 0; i < LJ_CHANS; i++) if (s->state[i] != LJ_OFF) return 1;
    return 0;
}

int lj_sequence_alloc(lj_sequence *seq, uint16_t n)
{
    lj_scene *s;
    int i;
    if (n < 1 || n > LJ_SCENES_MAX) return -1;
    s = (lj_scene *)malloc((size_t)n * sizeof *s);
    if (!s) lj_fatal("lj_sequence_alloc: out of memory (%u scenes)", n);
    for (i = 0; i < n; i++) lj_scene_init(&s[i]);
    free(seq->scenes);
    seq->scenes = s; seq->n = n;
    return 0;
}

void lj_sequence_free_scenes(lj_sequence *seq)
{
    free(seq->scenes);
    seq->scenes = NULL; seq->n = 0;
}

/* Grows scenes by `count`, opening a gap at `at`; the gap is uninitialised. */
static int lj_sequence_grow(lj_sequence *seq, uint16_t at, uint16_t count)
{
    lj_scene *s;
    if (count < 1 || seq->n + count > LJ_SCENES_MAX || at > seq->n) return -1;
    s = (lj_scene *)realloc(seq->scenes, (size_t)(seq->n + count) * sizeof *s);
    if (!s) lj_fatal("lj_sequence_grow: out of memory (%u scenes)", seq->n + count);
    memmove(s + at + count, s + at, (size_t)(seq->n - at) * sizeof *s);
    seq->scenes = s; seq->n = (uint16_t)(seq->n + count);
    return 0;
}

int lj_sequence_add(lj_sequence *seq, uint16_t current)
{
    if (current >= seq->n || lj_sequence_grow(seq, seq->n, 1) != 0) return -1;
    lj_scene_copy(&seq->scenes[seq->n - 1], &seq->scenes[current]);
    return seq->n - 1;
}

int lj_sequence_insert(lj_sequence *seq, uint16_t before, uint16_t count, int copy_current)
{
    lj_scene src;
    int i;
    if (before >= seq->n) return -1;
    lj_scene_copy(&src, &seq->scenes[before]);          /* "current" is the scene the gap opens before; it moves up by count */
    if (lj_sequence_grow(seq, before, count) != 0) return -1;
    for (i = 0; i < count; i++) {
        if (copy_current) lj_scene_copy(&seq->scenes[before + i], &src);
        else lj_scene_init(&seq->scenes[before + i]);
    }
    return before;
}

int lj_sequence_delete(lj_sequence *seq, uint16_t first, uint16_t last)
{
    if (first > last || last >= seq->n) return -1;
    if (first == 0 && last == seq->n - 1) {              /* "deleting all scenes, resets the current sequence to one scene with default (all off) output" [03 s4.3] */
        lj_scene_init(&seq->scenes[0]);
        seq->n = 1;
        return 0;
    }
    memmove(seq->scenes + first, seq->scenes + last + 1, (size_t)(seq->n - last - 1) * sizeof *seq->scenes);
    seq->n = (uint16_t)(seq->n - (last - first + 1));
    return 0;
}

void lj_sequence_new(lj_sequence *seq)
{
    lj_scene *keep = seq->scenes;
    memset(seq, 0, sizeof *seq);
    seq->scenes = keep;
    lj_sequence_alloc(seq, 1);
}
