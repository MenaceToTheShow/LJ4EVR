/* lj_cuepage.c - the cue page set (lj_cuepage.h, 03 s5.8, F28). The file layer is the W1
 * lj_cue_pages_load/save below (CuePageInfo.Dat; merged here from lj_cue.c, ME-101). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <string.h>
#include "model/lj_cuepage.h"
#include "model/lj_kv.h"
#include "core/lj_str.h"

void lj_cuepages_init(lj_cuepages *p) { memset(p, 0, sizeof *p); p->cursor = -1; }

int lj_cuepages_load(const char *path, lj_cuepages *out, char *err, size_t cap)
{
    FILE *f = fopen(path, "rb");
    const char *bad;
    int n;
    lj_cuepages_init(out);
    if (!f) { if (err && cap) err[0] = 0; return 0; }   /* missing file = no pages */
    fclose(f);
    n = lj_cue_pages_load(path, out->page, LJ_CUE_PAGES);   /* range-checks the cue numbers (ME-77) */
    if (n < 0) { lj_cuepages_init(out); lj_kv_err(err, cap, path, 0, "bad CuePageInfo.Dat"); return -1; }
    out->n = n;
    if ((bad = lj_cuepages_validate(out)) != NULL) { lj_cuepages_init(out); lj_kv_err(err, cap, path, 0, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_cuepages_save(const char *path, const lj_cuepages *in) { return lj_cue_pages_save(path, in->page, in->n); }

const char *lj_cuepages_validate(const lj_cuepages *in)
{
    int i, j;
    if (in->n < 0 || in->n > LJ_CUE_PAGES) return "page count not 0..255";
    if (in->current < 0 || (in->n && in->current >= in->n) || (!in->n && in->current)) return "current page out of range";
    for (i = 0; i < in->n; i++) {
        const lj_cue_page *pg = &in->page[i];
        if (!memchr(pg->name, 0, sizeof pg->name)) return "page name not NUL-terminated";
        if (pg->n < 0 || pg->n > LJ_CUE_PAGES) return "cues per page not 0..255";
        for (j = 0; j < pg->n; j++) if (pg->cue[j] < 1 || pg->cue[j] > LJ_NUMBER_MAX) return "cue number not 1..99999999";
    }
    if (in->n && (in->cursor < -1 || in->cursor >= in->page[in->current].n)) return "cursor out of range";
    return NULL;
}

int lj_cuepages_insert(lj_cuepages *p, int before, const char *name)
{
    if (p->n >= LJ_CUE_PAGES || before < 0 || before > p->n) return -1;
    memmove(&p->page[before + 1], &p->page[before], (size_t)(p->n - before) * sizeof p->page[0]);
    memset(&p->page[before], 0, sizeof p->page[0]);
    lj_str_copy(p->page[before].name, sizeof p->page[before].name, name ? name : "");
    if (p->n && p->current >= before) p->current++;   /* the shown tab stays the same page */
    p->n++;
    return before;
}

int lj_cuepages_add(lj_cuepages *p, const char *name) { return lj_cuepages_insert(p, p->n, name); }

int lj_cuepages_delete(lj_cuepages *p, int index)
{
    if (index < 0 || index >= p->n) return -1;
    memmove(&p->page[index], &p->page[index + 1], (size_t)(p->n - index - 1) * sizeof p->page[0]);
    p->n--;
    if (p->current > index || p->current >= p->n) p->current--;
    if (p->current < 0) p->current = 0;
    p->cursor = -1;
    return 0;
}

int lj_cuepages_rename(lj_cuepages *p, int index, const char *name)
{
    if (index < 0 || index >= p->n || !name) return -1;
    lj_str_copy(p->page[index].name, sizeof p->page[index].name, name);   /* truncated to 30 bytes, like the edit box */
    return 0;
}

int lj_cuepages_clear(lj_cuepages *p, int index)
{
    if (index < 0 || index >= p->n) return -1;
    p->page[index].n = 0;
    if (index == p->current) p->cursor = -1;
    return 0;
}

int lj_cuepages_move(lj_cuepages *p, int from, int to)
{
    lj_cue_page t;
    if (from < 0 || from >= p->n || to < 0 || to >= p->n) return -1;
    if (from == to) return 0;
    t = p->page[from];
    if (from < to) memmove(&p->page[from], &p->page[from + 1], (size_t)(to - from) * sizeof t);
    else           memmove(&p->page[to + 1], &p->page[to], (size_t)(from - to) * sizeof t);
    p->page[to] = t;
    if (p->current == from) p->current = to;                                   /* the shown tab follows its page */
    else if (from < p->current && p->current <= to) p->current--;
    else if (to <= p->current && p->current < from) p->current++;
    return 0;
}

int lj_cuepages_add_cue(lj_cuepages *p, int index, uint32_t cue, int at)
{
    lj_cue_page *pg;
    int j;
    if (index < 0 || index >= p->n || cue < 1 || cue > LJ_NUMBER_MAX) return -1;
    pg = &p->page[index];
    for (j = 0; j < pg->n; j++) if (pg->cue[j] == cue) return 0;               /* never adds twice [03 s5.7] */
    if (pg->n >= LJ_CUE_PAGES) return -1;
    if (at < 0 || at > pg->n) at = pg->n;
    memmove(&pg->cue[at + 1], &pg->cue[at], (size_t)(pg->n - at) * sizeof pg->cue[0]);
    pg->cue[at] = cue;
    pg->n++;
    if (index == p->current && p->cursor >= at) p->cursor++;
    return 1;
}

int lj_cuepages_remove_cue(lj_cuepages *p, int index, int at)
{
    lj_cue_page *pg;
    if (index < 0 || index >= p->n) return -1;
    pg = &p->page[index];
    if (at < 0 || at >= pg->n) return -1;
    memmove(&pg->cue[at], &pg->cue[at + 1], (size_t)(pg->n - at - 1) * sizeof pg->cue[0]);
    pg->n--;
    if (index == p->current && p->cursor >= pg->n) p->cursor = pg->n - 1;
    return 0;
}

int lj_cuepages_step(lj_cuepages *p, int delta, int circular)
{
    int c;
    if (!p->n) return -1;
    c = p->current + delta;
    if (c < 0 || c >= p->n) {
        if (!circular) return -1;
        c = ((c % p->n) + p->n) % p->n;
    }
    p->current = c;
    p->cursor = -1;
    return c;
}

uint32_t lj_cuepages_step_cue(lj_cuepages *p, int delta)
{
    const lj_cue_page *pg;
    int c;
    if (!p->n || !delta) return 0;
    pg = &p->page[p->current];
    if (!pg->n) return 0;
    c = p->cursor < 0 ? (delta > 0 ? 0 : pg->n - 1) : p->cursor + delta;   /* nothing selected: Next = first, Previous = last */
    if (c < 0 || c >= pg->n) return 0;
    p->cursor = c;
    return pg->cue[c];
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
