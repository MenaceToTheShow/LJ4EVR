/* lj_groups.h - GroupInfo.dat (design 5.2: "LJ4 groups 1", "group n name=...
 * members=1,2,5"): the 99 user fixture groups [18 s8, 01 s3.4]. */
#ifndef LJ_MODEL_GROUPS_H
#define LJ_MODEL_GROUPS_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_groups_load(const char *path, lj_group groups[LJ_USER_GROUPS], char *err, size_t cap);
int         lj_groups_save(const char *path, const lj_group groups[LJ_USER_GROUPS]);
const char *lj_groups_validate(const lj_group groups[LJ_USER_GROUPS]);    /* NULL = valid: members 0/1, names NUL-terminated */

#endif /* LJ_MODEL_GROUPS_H */
