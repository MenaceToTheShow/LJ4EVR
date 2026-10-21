/* lj_cuelist.h - CueLists\NNNNNNNN.Cl2 and AutoSave.Cl2 (design 5.2: "LJ4 cl 1",
 * num name tracking, "mark k=line", then "line n adv=auto|manual cmd=<snake
 * case of 03 s6.3> param= cue= trig= comment=..."). W3 runs it; W1 only
 * declares the record so the status bar's CueList Modified flag and the
 * Startup Options "Cuelist #" field have a type. */
#ifndef LJ_MODEL_CUELIST_H
#define LJ_MODEL_CUELIST_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_cuelist_load(const char *path, lj_cuelist *out, char *err, size_t cap);   /* lines allocated (cap >= n); 0 / -1 */
int         lj_cuelist_save(const char *path, const lj_cuelist *in);
const char *lj_cuelist_validate(const lj_cuelist *in);
lj_cuelist *lj_cuelist_new_rec(void);
void        lj_cuelist_free(lj_cuelist *cl);                                              /* frees lines and the record; NULL is a no-op */
int         lj_cuelist_insert(lj_cuelist *cl, uint32_t before, const lj_cl_line *line);   /* grows lines; renumbers .line 1..n; 0 / -1 */
int         lj_cuelist_delete(lj_cuelist *cl, uint32_t index);

extern const char *const lj_cl_cmd_words[];   /* cmd= words in 03 s6.3 order, index = lj_cl_line.cmd ("none" first); NULL-terminated. W3 derives its enum from this order */

#endif /* LJ_MODEL_CUELIST_H */
