/* lj_bgcue.h - BGCues\NNNNNNNN.bgc (design 5.2: "LJ4 bgc 1", num name
 * transparent, "slot n seq= mode=auto|audio|noloop on="; extension PROPOSAL,
 * LJ2's is unknown [18 s2]). Max LJ_BG_FILES per library. W2 runs it. */
#ifndef LJ_MODEL_BGCUE_H
#define LJ_MODEL_BGCUE_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_bgcue_load(const char *path, lj_bgcue *out, char *err, size_t cap);
int         lj_bgcue_save(const char *path, const lj_bgcue *in);
const char *lj_bgcue_validate(const lj_bgcue *in);
lj_bgcue   *lj_bgcue_new_rec(void);
void        lj_bgcue_free(lj_bgcue *bg);                                         /* NULL is a no-op */
int         lj_bgcue_add_sequence(lj_bgcue *bg, uint32_t seq);                   /* first free slot, else the last slot [03 s7]; returns the slot 1..5 */

#endif /* LJ_MODEL_BGCUE_H */
