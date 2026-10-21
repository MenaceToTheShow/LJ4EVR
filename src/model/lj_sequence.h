/* lj_sequence.h - Sequences\NNNNNNNN.seq (design 5.2 grammar: "LJ4 seq 1",
 * num name type created modified, "sync=0 gst=0 gft=0", scenes=n, then per
 * scene "scene st= ft=", "ch l:a=v:S ..." (non-Off only), "fx n preset=",
 * "fx n macro=a,b,... mst=..."). Loaders build the record outside lj_lock and
 * the caller swaps it in under the lock (design 3.1). */
#ifndef LJ_MODEL_SEQUENCE_H
#define LJ_MODEL_SEQUENCE_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_sequence_load(const char *path, lj_sequence *out, char *err, size_t cap);   /* 0 / -1 with err = first problem (import boundary, design 2.0); out->scenes allocated */
int         lj_sequence_save(const char *path, const lj_sequence *in);                      /* 0 / -1 + log */
const char *lj_sequence_validate(const lj_sequence *in);                                   /* NULL = valid, else the first violation (n 1..999, times <= 60000, states 0..2, name NUL-terminated) */
void        lj_sequence_free(lj_sequence *seq);                                             /* frees scenes and the record (heap records from the lj_table); NULL is a no-op */
lj_sequence *lj_sequence_new_rec(void);                                                     /* heap record with one all-off scene; lj_fatal on OOM */

#endif /* LJ_MODEL_SEQUENCE_H */
