/* lj_sequence.c - Sequences\NNNNNNNN.seq (lj_sequence.h, design 5.2 grammar).
 * Loads build the record outside lj_lock; the caller swaps it in under the lock. */
#define _CRT_SECURE_NO_WARNINGS 1   /* localtime under /W4 /WX on MSVC */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "model/lj_sequence.h"
#include "model/lj_scene.h"
#include "model/lj_kv.h"

int lj_sequence_load(const char *path, lj_sequence *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP];
    int fmt, r, item = 0, cur = -1;
    long n = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "seq", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 seq file"); return -1; }
    lj_sequence_free_scenes(out);
    memset(out, 0, sizeof *out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "num") == 0) out->number = (uint32_t)strtoul(v, NULL, 10);
        else if (strcmp(k, "name") == 0) lj_kv_unquote(out->name, sizeof out->name, v);
        else if (strcmp(k, "type") == 0) lj_kv_unquote(out->type, sizeof out->type, v);
        else if (strcmp(k, "sync") == 0) {
            out->sync_times = (uint8_t)(strtol(v, NULL, 10) != 0);
            out->global_scene_time = (uint8_t)(lj_kv_long(v, "gst", 0) != 0);
            out->global_fade_time = (uint8_t)(lj_kv_long(v, "gft", 0) != 0);
        } else if (strcmp(k, "scenes") == 0) {
            n = strtol(v, NULL, 10);
            if (out->scenes || n < 1 || n > LJ_SCENES_MAX) { bad = "scenes= must be 1..999 and appear once"; break; }
            lj_sequence_alloc(out, (uint16_t)n);
        } else if (strcmp(k, "scene") == 0) {
            if (!out->scenes || ++cur >= n) { bad = "more scene lines than scenes="; break; }
            if (lj_kv_scene_field(&out->scenes[cur], k, v) < 0) { bad = "bad scene line"; break; }
        } else if (strcmp(k, "ch") == 0 || strcmp(k, "fx") == 0) {
            if (cur < 0) { bad = "channel data before the first scene line"; break; }
            if (lj_kv_scene_field(&out->scenes[cur], k, v) < 0) { bad = "bad channel or fx line"; break; }
        }                                                  /* created=, modified= and unknown keys: skipped (design 5.2) */
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad && cur + 1 != n) bad = "scene count does not match scenes=";
    if (!bad) bad = lj_sequence_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); lj_sequence_free_scenes(out); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_sequence_save(const char *path, const lj_sequence *in)
{
    char q[72], stamp[32];
    int i, ok;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    lj_rec *rec = lj_rec_open_write(path, "seq", 1);
    if (!rec) return -1;
    if (!tm || strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", tm) == 0) stamp[0] = 0;
    ok = lj_rec_write(rec, "num", "%u", (unsigned)in->number) == 0;
    lj_kv_quote(q, sizeof q, in->name); ok = ok && lj_rec_write(rec, "name", "%s", q) == 0;
    lj_kv_quote(q, sizeof q, in->type); ok = ok && lj_rec_write(rec, "type", "%s", q) == 0;
    ok = ok && lj_rec_write(rec, "modified", "%s", stamp) == 0;
    ok = ok && lj_rec_write(rec, "sync", "%d gst=%d gft=%d", in->sync_times, in->global_scene_time, in->global_fade_time) == 0;
    ok = ok && lj_rec_write(rec, "scenes", "%u", in->n) == 0;
    for (i = 0; ok && i < in->n; i++) ok = lj_kv_scene_write(rec, &in->scenes[i]) == 0;
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_sequence_validate(const lj_sequence *in)
{
    int i, c;
    if (!memchr(in->name, 0, sizeof in->name) || !memchr(in->type, 0, sizeof in->type)) return "name or type not NUL-terminated";
    if (in->n < 1 || in->n > LJ_SCENES_MAX || !in->scenes) return "scene count not 1..999";
    for (i = 0; i < in->n; i++) {
        const lj_scene *s = &in->scenes[i];
        if (s->scene_ms > LJ_TIME_MAX_MS || s->fade_ms > LJ_TIME_MAX_MS) return "scene or fade time over 60000 ms";
        for (c = 0; c < LJ_CHANS; c++) if (s->state[c] > LJ_FADE) return "fade state not 0..2";
        for (c = 0; c < LJ_FIXTURES; c++) {
            int j;
            if (s->preset[c] > LJ_PRESETS) return "preset reference not 0..99";
            for (j = 0; j < 6; j++) if (s->macro[c].fstate[j] > LJ_FADE) return "macro fade state not 0..2";
        }
    }
    return NULL;
}

void lj_sequence_free(lj_sequence *seq) { if (!seq) return; lj_sequence_free_scenes(seq); free(seq); }

lj_sequence *lj_sequence_new_rec(void)
{
    lj_sequence *s = (lj_sequence *)calloc(1, sizeof *s);
    if (!s) lj_fatal("lj_sequence_new_rec: out of memory");
    lj_sequence_new(s);
    return s;
}
