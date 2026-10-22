/* lj_cuepage.c - the cue page set (lj_cuepage.h, 03 s5.8, F28). The file layer is the W1
 * lj_cue_pages_load/save of lj_cue.c (CuePageInfo.Dat). */
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
