/* lj_palettes.h - Palettes\<Control>.Pre (design 5.2: "LJ4 palette 1", "entry n
 * name=" + "ch l:a=v:S" lines): the user palettes of one control kind, a
 * growable array of lj_user_palette [04 s9]. Only Snap/Fade entries apply;
 * values copy only between fixtures of the same profile id [15/00 s5]. */
#ifndef LJ_MODEL_PALETTES_H
#define LJ_MODEL_PALETTES_H

#include <stddef.h>
#include "model/lj_model.h"

typedef struct { lj_user_palette *e; int n, cap; } lj_palette_list;

int         lj_palettes_load(const char *path, lj_palette_list *out, char *err, size_t cap);   /* allocates out->e; 0 / -1 */
int         lj_palettes_save(const char *path, const lj_palette_list *in);
const char *lj_palettes_validate(const lj_palette_list *in);
int         lj_palettes_add(lj_palette_list *l, const char *name, const uint8_t val[LJ_CHANS], const uint8_t state[LJ_CHANS]);   /* returns the index or -1 */
int         lj_palettes_delete(lj_palette_list *l, int index);
void        lj_palettes_free(lj_palette_list *l);
const char *lj_palettes_file_name(enum lj_ctl_kind kind);                                       /* "Intensity.Pre" ... per control kind; NULL for kinds without palettes */

#endif /* LJ_MODEL_PALETTES_H */
