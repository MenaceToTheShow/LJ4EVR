/* lj_genmacro.h - CueMacros\NNNNNNNN.CMA (design 5.2: "LJ4 macro 1", num name
 * restart, "ch l:a shape=7 bon=1 base=100 amp=45,45 cycle=168 dir=fwd delay=0"
 * per channel with a shape; cycle in deciseconds as the struct holds it, 14 sA).
 * W2 runs it (lj_genmacro_data.c is the shape table). */
#ifndef LJ_MODEL_GENMACRO_H
#define LJ_MODEL_GENMACRO_H

#include <stddef.h>
#include "model/lj_model.h"

int          lj_genmacro_load(const char *path, lj_genmacro *out, char *err, size_t cap);
int          lj_genmacro_save(const char *path, const lj_genmacro *in);
const char  *lj_genmacro_validate(const lj_genmacro *in);                 /* NULL = valid: dir fwd/rev, name NUL-terminated */
lj_genmacro *lj_genmacro_new_rec(void);                                   /* heap, zeroed (free with free()) */

#endif /* LJ_MODEL_GENMACRO_H */
