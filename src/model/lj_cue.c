/* lj_cue.c - Cues\NNNNNNNN.cue and Cues\CuePageInfo.Dat (lj_cue.h, design 5.2). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_cue.h"
#include "core/lj_str.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"

static void lj_cue_init(lj_cue *c)                 /* every slot empty, spt/atpt 1, cue time 100, macro 100/100 */
{
    int i;
    memset(c, 0, sizeof *c);
    c->cue_time_pct = 100; c->macro_amp_pct = 100; c->macro_speed_pct = 100;
    for (i = 0; i < LJ_CUE_SLOTS; i++) c->slot[i].scenes_per_trig = c->slot[i].audio_trigs_per_trig = 1;
}

int lj_cue_load(const char *path, lj_cue *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0, rbad = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "cue", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 cue file"); return -1; }
    lj_cue_init(out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "num") == 0) out->number = (uint32_t)lj_kv_range_lead(v, 0, 0, LJ_NUMBER_MAX, &rbad);
        else if (strcmp(k, "name") == 0) lj_kv_unquote(out->name, sizeof out->name, v);
        else if (strcmp(k, "type") == 0) lj_kv_unquote(out->type, sizeof out->type, v);
        else if (strcmp(k, "cuetime") == 0) out->cue_time_pct = (uint16_t)lj_kv_range_lead(v, 100, 1, 999, &rbad);   /* ME-77: 65636 fails, never aliases to 100 */
        else if (strcmp(k, "macro") == 0) {
            uint32_t m[2];
            if (lj_kv_list(v, m, 2) != 2 || m[0] > 100 || m[1] > 100) { bad = "macro= needs amp,speed 0..100"; break; }
            out->macro_amp_pct = (uint8_t)m[0]; out->macro_speed_pct = (uint8_t)m[1];
            out->save_macro_values = (uint8_t)lj_kv_range(v, "savemacro", 0, 0, 1, &rbad);
        } else if (strcmp(k, "transparent") == 0) out->transparent = (uint8_t)lj_kv_range_lead(v, 0, 0, 1, &rbad);
        else if (strcmp(k, "bgcue") == 0) {
            out->bg_cue = (uint32_t)lj_kv_range_lead(v, 0, 0, LJ_NUMBER_MAX, &rbad);
            out->gen_macro = (uint32_t)lj_kv_range(v, "genmacro", 0, 0, LJ_MACROS_MAX, &rbad);
        } else if (strcmp(k, "slot") == 0) {
            long i = strtol(v, &e, 10);
            lj_cue_slot *s;
            if (e == v || i < 1 || i > LJ_CUE_SLOTS) { bad = "slot index not 1..12"; break; }
            s = &out->slot[i - 1];
            s->seq = (uint32_t)lj_kv_range(v, "seq", 0, 0, LJ_NUMBER_MAX, &rbad);
            s->enabled = (uint8_t)lj_kv_range(v, "on", 1, 0, 1, &rbad);
            s->trig = (uint8_t)lj_kv_word(v, "trig", lj_trig_words, LJ_TRIG_AUTO);
            s->dir = (uint8_t)lj_kv_word(v, "dir", lj_dir_words, LJ_DIR_FWD);
            s->scenes_per_trig = (uint8_t)lj_kv_range(v, "spt", 1, 0, 255, &rbad);          /* ME-77: 257 fails; 0 = 1 below (v0.1.0 wrote spt=0 from Add Sequence to Current Cue) */
            s->audio_trigs_per_trig = (uint8_t)lj_kv_range(v, "atpt", 1, 0, 255, &rbad);
            if (!s->scenes_per_trig) s->scenes_per_trig = 1;
            if (!s->audio_trigs_per_trig) s->audio_trigs_per_trig = 1;
            s->trig_on_load = (uint8_t)lj_kv_range(v, "tol", 0, 0, 1, &rbad);
            s->first = (uint16_t)lj_kv_range(v, "first", 0, 0, LJ_SCENES_MAX, &rbad);
            s->last = (uint16_t)lj_kv_range(v, "last", 0, 0, LJ_SCENES_MAX, &rbad);
            s->loops = (uint16_t)lj_kv_range(v, "loops", 0, 0, 65535, &rbad);
            s->end_action = (uint8_t)lj_kv_word(v, "end", lj_end_words, LJ_END_STOP);
            s->goto_scene = (uint16_t)lj_kv_range(v, "goto", 0, 0, LJ_SCENES_MAX, &rbad);
            s->link_cue = (uint32_t)lj_kv_range(v, "link", 0, 0, LJ_NUMBER_MAX, &rbad);
            s->time_pct = (uint16_t)lj_kv_range(v, "time", 0, 0, 999, &rbad);
            s->chase_ms = (uint32_t)lj_kv_range(v, "chase", 0, 0, LJ_TIME_MAX_MS, &rbad);
        }
        if (rbad) { bad = "number out of range"; break; }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_cue_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_cue_save(const char *path, const lj_cue *in)
{
    char q[72];
    int i, ok;
    lj_rec *rec = lj_rec_open_write(path, "cue", 1);
    if (!rec) return -1;
    ok = lj_rec_write(rec, "num", "%u", (unsigned)in->number) == 0;
    lj_kv_quote(q, sizeof q, in->name); ok = ok && lj_rec_write(rec, "name", "%s", q) == 0;
    lj_kv_quote(q, sizeof q, in->type); ok = ok && lj_rec_write(rec, "type", "%s", q) == 0;
    ok = ok && lj_rec_write(rec, "cuetime", "%u", in->cue_time_pct) == 0;
    ok = ok && lj_rec_write(rec, "macro", "%d,%d savemacro=%d", in->macro_amp_pct, in->macro_speed_pct, in->save_macro_values) == 0;
    ok = ok && lj_rec_write(rec, "transparent", "%d", in->transparent) == 0;
    ok = ok && lj_rec_write(rec, "bgcue", "%u genmacro=%u", (unsigned)in->bg_cue, (unsigned)in->gen_macro) == 0;
    for (i = 0; ok && i < LJ_CUE_SLOTS; i++) {
        const lj_cue_slot *s = &in->slot[i];
        if (!s->seq) continue;
        ok = lj_rec_write(rec, "slot ", "%d seq=%u on=%d trig=%s dir=%s spt=%d atpt=%d tol=%d first=%u last=%u loops=%u end=%s goto=%u link=%u time=%u chase=%u",
                          i + 1, (unsigned)s->seq, s->enabled, lj_word(lj_trig_words, s->trig), lj_word(lj_dir_words, s->dir),
                          s->scenes_per_trig, s->audio_trigs_per_trig, s->trig_on_load, s->first, s->last, s->loops,
                          lj_word(lj_end_words, s->end_action), s->goto_scene, (unsigned)s->link_cue, s->time_pct, (unsigned)s->chase_ms) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_cue_validate(const lj_cue *in)
{
    int i;
    if (!memchr(in->name, 0, sizeof in->name) || !memchr(in->type, 0, sizeof in->type)) return "name or type not NUL-terminated";
    if (in->cue_time_pct < 1 || in->cue_time_pct > 999) return "cue time not 1..999 %";
    if (in->macro_amp_pct > 100 || in->macro_speed_pct > 100) return "macro amplitude or speed over 100 %";
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        const lj_cue_slot *s = &in->slot[i];
        if (!s->seq) {
            if (s->enabled) return "slot enabled without a sequence";
            continue;
        }
        if (s->trig > LJ_TRIG_AUDIO) return "trig mode not auto/manual/audio";
        if (s->dir > LJ_DIR_RANDOM) return "direction not fwd/rev/bounce/rnd";
        if (s->end_action > LJ_END_LINK) return "end action not stop/out/goto/link";
        if (s->time_pct > 999) return "slot time not 0..999 %";
        if (s->first > LJ_SCENES_MAX || s->last > LJ_SCENES_MAX || (s->last && s->first > s->last)) return "first scene after last scene";
        if (s->loops && s->dir == LJ_DIR_RANDOM) return "loop count in random mode";   /* 03 s5.5 "not available in random mode" */
        if (s->goto_scene > LJ_SCENES_MAX) return "goto scene over 999";
        if (s->chase_ms > LJ_TIME_MAX_MS) return "chase time over 60000 ms";
        if (s->scenes_per_trig < 1 || s->audio_trigs_per_trig < 1) return "scenes/trig and audio-trigs/trig are at least 1";
    }
    return NULL;
}

lj_cue *lj_cue_new_rec(void)
{
    lj_cue *c = (lj_cue *)malloc(sizeof *c);
    if (!c) lj_fatal("lj_cue_new_rec: out of memory");
    lj_cue_init(c);
    return c;
}

void lj_cue_free(lj_cue *cue) { free(cue); }

int lj_cue_find_free(const lj_cue *cue)
{
    int i;
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (!cue->slot[i].seq) return i + 1;
    return -1;
}

int lj_cue_add_sequence(lj_cue *cue, uint32_t seq, int slot)
{
    lj_cue_slot *s;
    if (slot == 0) slot = lj_cue_find_free(cue);
    if (slot < 1 || slot > LJ_CUE_SLOTS || !seq) return -1;
    s = &cue->slot[slot - 1];
    memset(s, 0, sizeof *s);
    s->seq = seq; s->enabled = 1; s->scenes_per_trig = s->audio_trigs_per_trig = 1;
    return slot;
}

int lj_cue_pages_load(const char *path, lj_cue_page *out, int max)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP];
    int fmt, r, n = 0, item = 0, j;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "cuepages", &fmt);
    if (!rec) return -1;
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "page") != 0) continue;
        if (lj_kv_range_lead(v, 0, n + 1, n + 1, NULL) != n + 1) { bad = "page index out of order"; break; }   /* ME-77 helper */
        if (n >= max) { bad = "more pages than the caller holds"; break; }
        memset(&out[n], 0, sizeof out[n]);
        if (lj_kv_get(v, "name", q, sizeof q) == 1) lj_str_copy(out[n].name, sizeof out[n].name, q);
        if (lj_kv_get(v, "cues", q, sizeof q) == 1 && q[0]) {
            out[n].n = lj_kv_list(q, out[n].cue, LJ_CUE_PAGES);
            if (out[n].n < 0) { bad = "bad cues= list"; break; }
            for (j = 0; j < out[n].n; j++) if (out[n].cue[j] < 1 || out[n].cue[j] > LJ_NUMBER_MAX) break;
            if (j < out[n].n) { bad = "cue number not 1..99999999"; break; }   /* ME-77: no alias of an over-long number */
        }
        n++;
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(NULL, 0, path, item, bad); return -1; }
    return n;
}

int lj_cue_pages_save(const char *path, const lj_cue_page *in, int n)
{
    char q[72], list[LJ_KV_VAL_CAP];
    int i, j, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "cuepages", 1);
    if (!rec) return -1;
    for (i = 0; ok && i < n; i++) {
        size_t len = 0;
        list[0] = 0;
        for (j = 0; j < in[i].n && j < LJ_CUE_PAGES; j++)
            len += (size_t)snprintf(list + len, sizeof list - len, "%s%u", j ? "," : "", (unsigned)in[i].cue[j]);
        lj_kv_quote(q, sizeof q, in[i].name);
        ok = lj_rec_write(rec, "page ", "%d name=%s cues=%s", i + 1, q, list) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}
