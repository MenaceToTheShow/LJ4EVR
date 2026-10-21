/* lj_table.c - sorted number -> record array (lj_table.h). UI thread only. */
#include <stdlib.h>
#include <string.h>
#include "model/lj_table.h"
#include "core/lj_types.h"

void lj_table_init(lj_table *t) { memset(t, 0, sizeof *t); }

static int lj_table_find(const lj_table *t, uint32_t number)   /* index of number, or ~(insert position) */
{
    int lo = 0, hi = t->n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (t->e[mid].number == number) return mid;
        if (t->e[mid].number < number) lo = mid + 1; else hi = mid - 1;
    }
    return ~lo;
}

void *lj_table_get(const lj_table *t, uint32_t number)
{
    int i = lj_table_find(t, number);
    return i >= 0 ? t->e[i].rec : NULL;
}

int lj_table_put(lj_table *t, uint32_t number, void *rec)
{
    int i;
    if (!rec || number == 0) return -1;
    i = lj_table_find(t, number);
    if (i >= 0) { t->e[i].rec = rec; return 0; }
    i = ~i;
    if (t->n == t->cap) {
        int cap = t->cap ? t->cap * 2 : 64;
        lj_table_entry *e = (lj_table_entry *)realloc(t->e, (size_t)cap * sizeof *e);
        if (!e) lj_fatal("lj_table: out of memory at %d entries", cap);
        t->e = e; t->cap = cap;
    }
    memmove(t->e + i + 1, t->e + i, (size_t)(t->n - i) * sizeof *t->e);
    t->e[i].number = number; t->e[i].rec = rec; t->n++;
    return 0;
}

void *lj_table_remove(lj_table *t, uint32_t number)
{
    void *rec;
    int i = lj_table_find(t, number);
    if (i < 0) return NULL;
    rec = t->e[i].rec;
    memmove(t->e + i, t->e + i + 1, (size_t)(t->n - i - 1) * sizeof *t->e);
    t->n--;
    return rec;
}

int lj_table_count(const lj_table *t) { return t->n; }

void *lj_table_at(const lj_table *t, int i, uint32_t *number)
{
    if (i < 0 || i >= t->n) return NULL;
    if (number) *number = t->e[i].number;
    return t->e[i].rec;
}

void lj_table_free(lj_table *t, void (*free_rec)(void *rec))
{
    int i;
    if (free_rec) for (i = 0; i < t->n; i++) free_rec(t->e[i].rec);
    free(t->e);
    memset(t, 0, sizeof *t);
}
