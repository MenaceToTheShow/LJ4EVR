/* lj_statics.h - Statics.dat (design 5.2: "LJ4 statics 1", "page n name=",
 * "slot n name= type= act= rep= ft= level= groups= kind= link=<seq>,<scene>"
 * plus the recorded scene's "ch" lines). Pages are LJ_STATICS2_SLOTS_PER_PAGE
 * slots each (14 sB.8 item 2 PROPOSAL); recorded slots own their lj_scene.
 * W2 runs it (layer 4). */
#ifndef LJ_MODEL_STATICS_H
#define LJ_MODEL_STATICS_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_statics_load(const char *path, lj_static_page **pages, int *npages, char *err, size_t cap);   /* allocates *pages (and each recorded slot's scene); 0 / -1 */
int         lj_statics_save(const char *path, const lj_static_page *pages, int npages);
const char *lj_statics_validate(const lj_static_page *pages, int npages);
void        lj_statics_free(lj_static_page *pages, int npages);                   /* scenes then the array */
int         lj_statics_add_page(lj_static_page **pages, int *npages, const char *name);   /* returns the new page index or -1 at LJ_STATIC_PAGES_MAX */
int         lj_static_slot_record(lj_static_slot *slot, const lj_scene *scene);  /* "Record Current Scene": copies into an owned scene, kind = RECORDED */
void        lj_static_slot_clear(lj_static_slot *slot);                          /* frees the scene, kind = EMPTY, name kept */

#endif /* LJ_MODEL_STATICS_H */
