/* lj_cuelist.c - CueLists\NNNNNNNN.Cl2 (lj_cuelist.h, design 5.2). W1 ships the
 * file and the line edits; W3 runs it. */
#include <stdlib.h>
#include <string.h>
#include "model/lj_cuelist.h"
#include "model/lj_kv.h"
#include "core/lj_str.h"

/* The cmd= words, in the order of 03 s6.3 (index = lj_cl_line.cmd; W3 derives its enum from this order). */
const char *const lj_cl_cmd_words[] = {
    "none", "wait_cd", "cd_play_from_track", "cd_play_from_time", "cd_stop",
    "wait_da", "play_da_file", "cue_da_file", "da_stop", "seek_da", "da_play", "da_timecode_factor",
    "mp_load_file", "mp_play", "mp_seek", "mp_stop", "mp_pause", "mp_wait", "mp_close_clip", "mp_start", "mp_close",
    "wait_mtc", "autoseek_mtc", "wait_elapsed", "wait_pc_clock", "wait_date_change",
    "goto_line", "execute_file", "set_current_cue", "set_cue_trig", "hotkey_command",
    "flash_sequence", "load_background_cue", "link_cuelist", NULL };
static const char *const lj_cl_adv_words[] = { "auto", "manual", NULL };

static int lj_cuelist_reserve(lj_cuelist *cl, uint32_t n)
{
    lj_cl_line *l;
    uint32_t cap;
    if (n <= cl->cap) return 0;
    cap = cl->cap ? cl->cap * 2 : 16;
    if (cap < n) cap = n;
    l = (lj_cl_line *)realloc(cl->lines, (size_t)cap * sizeof *l);
    if (!l) lj_fatal("lj_cuelist: out of memory at %u lines", (unsigned)cap);
    cl->lines = l; cl->cap = cap;
    return 0;
}

int lj_cuelist_load(const char *path, lj_cuelist *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "cl", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 cl file"); return -1; }
    free(out->lines);                                   /* out is a zeroed or valid record */
    memset(out, 0, sizeof *out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "num") == 0) out->number = (uint32_t)strtoul(v, NULL, 10);
        else if (strcmp(k, "name") == 0) lj_kv_unquote(out->name, sizeof out->name, v);
        else if (strcmp(k, "tracking") == 0) out->tracking = (uint8_t)(strtol(v, NULL, 10) != 0);
        else if (strcmp(k, "mark") == 0) {
            long i = strtol(v, &e, 10);
            if (e == v || i < 1 || i > LJ_MARKS || *e != '=') { bad = "mark index not 1..4"; break; }
            out->mark[i - 1] = (uint32_t)strtoul(e + 1, NULL, 10);
        } else if (strcmp(k, "line") == 0) {
            lj_cl_line *l;
            int w;
            if (strtol(v, &e, 10) != (long)out->n + 1 || e == v) { bad = "line index out of order"; break; }
            lj_cuelist_reserve(out, out->n + 1);
            l = &out->lines[out->n];
            memset(l, 0, sizeof *l);
            l->line = out->n + 1;
            l->manual = (uint8_t)lj_kv_word(v, "adv", lj_cl_adv_words, LJ_CL_AUTO);
            w = lj_kv_word(v, "cmd", lj_cl_cmd_words, 0);
            l->cmd = (uint8_t)w;
            l->param_ms = (uint32_t)lj_kv_long(v, "param", 0);
            if (lj_kv_get(v, "pstr", q, sizeof q) == 1) lj_str_copy(l->param, sizeof l->param, q);
            l->cue = (uint32_t)lj_kv_long(v, "cue", 0);
            l->trig_mask = (uint16_t)lj_kv_long(v, "trig", 0);
            if (lj_kv_get(v, "comment", q, sizeof q) == 1) lj_str_copy(l->comment, sizeof l->comment, q);
            out->n++;
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_cuelist_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); free(out->lines); out->lines = NULL; out->n = out->cap = 0; return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_cuelist_save(const char *path, const lj_cuelist *in)
{
    char q[72], p[2 * sizeof in->lines->param + 2], c[2 * sizeof in->lines->comment + 2];
    uint32_t i;
    int ok;
    lj_rec *rec = lj_rec_open_write(path, "cl", 1);
    if (!rec) return -1;
    ok = lj_rec_write(rec, "num", "%u", (unsigned)in->number) == 0;
    lj_kv_quote(q, sizeof q, in->name); ok = ok && lj_rec_write(rec, "name", "%s", q) == 0;
    ok = ok && lj_rec_write(rec, "tracking", "%d", in->tracking) == 0;
    for (i = 0; ok && i < LJ_MARKS; i++) if (in->mark[i]) ok = lj_rec_write(rec, "mark ", "%u=%u", (unsigned)i + 1, (unsigned)in->mark[i]) == 0;
    for (i = 0; ok && i < in->n; i++) {
        const lj_cl_line *l = &in->lines[i];
        lj_kv_quote(p, sizeof p, l->param);
        lj_kv_quote(c, sizeof c, l->comment);
        ok = lj_rec_write(rec, "line ", "%u adv=%s cmd=%s param=%u pstr=%s cue=%u trig=%u comment=%s", (unsigned)i + 1,
                          lj_cl_adv_words[l->manual ? 1 : 0], lj_cl_cmd_words[l->cmd], (unsigned)l->param_ms, p, (unsigned)l->cue, l->trig_mask, c) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_cuelist_validate(const lj_cuelist *in)
{
    uint32_t i;
    int ncmd = 0;
    while (lj_cl_cmd_words[ncmd]) ncmd++;
    if (!memchr(in->name, 0, sizeof in->name)) return "name not NUL-terminated";
    if (in->n && !in->lines) return "lines missing";
    for (i = 0; i < in->n; i++) {
        const lj_cl_line *l = &in->lines[i];
        if (l->line != i + 1) return "line numbers not 1..n";
        if (l->manual > LJ_CL_MANUAL) return "advance mode not auto/manual";
        if (l->cmd >= ncmd) return "unknown command";
        if (!memchr(l->param, 0, sizeof l->param) || !memchr(l->comment, 0, sizeof l->comment)) return "parameter or comment not NUL-terminated";
        if (l->cue && l->trig_mask) return "a line latches a cue or trigs slots, not both";   /* 03 s6.1 */
    }
    for (i = 0; i < LJ_MARKS; i++) if (in->mark[i] > in->n) return "mark beyond the last line";
    return NULL;
}

lj_cuelist *lj_cuelist_new_rec(void)
{
    lj_cuelist *cl = (lj_cuelist *)calloc(1, sizeof *cl);
    if (!cl) lj_fatal("lj_cuelist_new_rec: out of memory");
    return cl;
}

void lj_cuelist_free(lj_cuelist *cl) { if (!cl) return; free(cl->lines); free(cl); }

static void lj_cuelist_renumber(lj_cuelist *cl) { uint32_t i; for (i = 0; i < cl->n; i++) cl->lines[i].line = i + 1; }

int lj_cuelist_insert(lj_cuelist *cl, uint32_t before, const lj_cl_line *line)
{
    if (before > cl->n) before = cl->n;
    lj_cuelist_reserve(cl, cl->n + 1);
    memmove(cl->lines + before + 1, cl->lines + before, (size_t)(cl->n - before) * sizeof *cl->lines);
    cl->lines[before] = *line;
    cl->n++;
    lj_cuelist_renumber(cl);
    return 0;
}

int lj_cuelist_delete(lj_cuelist *cl, uint32_t index)
{
    if (index >= cl->n) return -1;
    memmove(cl->lines + index, cl->lines + index + 1, (size_t)(cl->n - index - 1) * sizeof *cl->lines);
    cl->n--;
    lj_cuelist_renumber(cl);
    return 0;
}
