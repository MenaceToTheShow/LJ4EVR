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


/* ---- W2 additions (14 sB): Statics 2 group names, the visible-slot count and the scope helpers ----
 * Statics.dat gains "group n name=" (n = 1..LJ_STATIC_GROUPS, the bit of lj_static_slot.groups) and
 * "n=<visible>" on the page line (ME-30: Visible Slots 1..LJ_STATICS2_SLOTS_MAX); the W1 reader skips both.
 * Every numeric field goes through lj_kv_range (ME-77): level 0..100, ft 0..60000, link scene 0..998, n 1..64. */
enum { LJ_STATIC_GROUPS = 64 /* the width of lj_static_slot.groups (uint64_t); 14 sB.5 "%d Groups", PROPOSAL rig 14 sB.8 item 6 */ };   /* LJ_STATICS2_SLOTS_MAX (64, ME-30): model/lj_model.h */
typedef struct { char name[LJ_STATIC_GROUPS][64]; } lj_static_groups;
int  lj_statics_groups_load(const char *path, lj_static_groups *out, char *err, size_t cap);   /* the "group" lines; absent = "Group %d" [14 sB.3 vocabulary] */
int  lj_statics_groups_save(const char *path, const lj_static_page *pages, int npages, const lj_static_groups *groups);   /* rewrites Statics.dat with pages, slots, scenes and group names */
int  lj_static_slot_link(lj_static_slot *slot, uint32_t seq, uint16_t scene);  /* "Link to current Sequence/Scene": kind = LINK, the owned scene freed [14 sB.6] */
int  lj_static_global_index(int page, int slot);                               /* (page-1) * LJ_STATICS2_SLOTS_MAX + (slot-1): the `a` of LJ_CMD_STATIC_*; -1 out of range (pages 1..64, slots 1..64) */
int  lj_static_scope_hits(const lj_static_page *pages, int npages, int actor, int other);   /* 1 when activating global slot `actor` with its lj_replace scope releases global slot `other` (same slot row / shared group bit / same page / all; the Solo scopes read identically in W2, PROPOSAL rig 14 sB.8 item 3) */

#endif /* LJ_MODEL_STATICS_H */
