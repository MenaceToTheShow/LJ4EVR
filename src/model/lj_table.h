/* lj_table.h - number -> record pointer map (design 4: lj_show.sequences, cues,
 * cuelists, bgcues, macros). UI thread only: records are loaded, inserted and
 * freed by the UI thread (design 2.3 rule 2, 6); the engine never calls
 * lj_table_* (tools/test_layering.py greps engine/ for it). A sorted array:
 * the library holds at most 9999 records per kind, lookups are UI-rate. */
#ifndef LJ_MODEL_TABLE_H
#define LJ_MODEL_TABLE_H

#include <stdint.h>

typedef struct { uint32_t number; void *rec; } lj_table_entry;
typedef struct { lj_table_entry *e; int n, cap; } lj_table;    /* sorted by number; zero-initialised = empty */

void  lj_table_init(lj_table *t);                                  /* empty; same as memset 0 */
void *lj_table_get(const lj_table *t, uint32_t number);            /* NULL when absent */
int   lj_table_put(lj_table *t, uint32_t number, void *rec);       /* insert or replace; 0 / -1 (rec NULL or number 0); growth failure is lj_fatal */
void *lj_table_remove(lj_table *t, uint32_t number);               /* unlinks and returns the record for the caller to free (under lj_lock, after the engine was handed a replacement: 2.3 rule 6); NULL when absent */
int   lj_table_count(const lj_table *t);
void *lj_table_at(const lj_table *t, int i, uint32_t *number);     /* i-th record in ascending number order; NULL when out of range */
void  lj_table_free(lj_table *t, void (*free_rec)(void *rec));     /* frees every record through free_rec (NULL = leave them) and the array; t is empty afterwards */

#endif /* LJ_MODEL_TABLE_H */
