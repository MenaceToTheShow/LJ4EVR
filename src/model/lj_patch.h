/* lj_patch.h - the global patch (02 s7.1): per output channel source, DMX-in
 * mode, invert, min/max, HTP group membership. Saved/loaded as a patch file
 * from the Global Patch dialog's File menu (design 5.2 grammar: "LJ4 patch 1",
 * "ch l:a src=none|out|in sl= sc= mode= inv= min= max= htp=<mask>"; only
 * channels that differ from the default are written). Consumed by S1, S5, S6. */
#ifndef LJ_MODEL_PATCH_H
#define LJ_MODEL_PATCH_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_patch_load(const char *path, lj_patch_ch patch[LJ_CHANS], char *err, size_t cap);
int         lj_patch_save(const char *path, const lj_patch_ch patch[LJ_CHANS]);
const char *lj_patch_validate(const lj_patch_ch patch[LJ_CHANS]);        /* NULL = valid: src enums, src_link 1..4, src_ch 1..512, min <= max, no self-patch */
void        lj_patch_init(lj_patch_ch patch[LJ_CHANS]);                  /* src none, in_mode abs, invert 0, min 0, max 255, htp_mask 0 */
int         lj_patch_is_default(const lj_patch_ch *p);

#endif /* LJ_MODEL_PATCH_H */
