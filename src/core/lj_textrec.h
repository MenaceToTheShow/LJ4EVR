/* lj_textrec.h - the LJ4 text record reader/writer (design 2.1, grammar in design 5.2).
 * A record file is line-oriented "key=value" text, CP1252, opened with a kind
 * ("seq", "cue", ...) and an integer format version. Unknown keys are skipped by
 * the callers, not here. */
#ifndef LJ_CORE_TEXTREC_H
#define LJ_CORE_TEXTREC_H

#include <stddef.h>
#include "core/lj_types.h"

typedef struct lj_rec lj_rec;                       /* one text record file, design 5 */

lj_rec *lj_rec_open_read(const char *path, const char *kind, int *format);         /* NULL + log when the header is not "LJ4 <kind> <format>" */
int     lj_rec_next(lj_rec *rec, char *key, size_t kcap, char *val, size_t vcap);  /* 1 = a key/value produced, 0 = EOF, -1 = error (the iterator model, design 2.0) */
lj_rec *lj_rec_open_write(const char *path, const char *kind, int format);         /* writes the header line; NULL + log when path holds this kind at a higher format (G2) */
int     lj_rec_write(lj_rec *rec, const char *key, const char *fmt, ...) LJ_PRINTF(3, 4);   /* one "key=value" line; 0 / -1. A key ending in ' ' is a list head written without '=': ("scene ", "st=%d ft=%d") -> "scene st=1000 ft=50"; the reader splits at the first '=' or ' ' */
int     lj_rec_close(lj_rec *rec);                                                 /* flushes; -1 when the final write failed; NULL is a no-op */

#endif /* LJ_CORE_TEXTREC_H */
