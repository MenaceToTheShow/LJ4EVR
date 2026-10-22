/* lj_cuepage.h - cue pages (W2; 03 s5.8, F28): up to LJ_CUE_PAGES user-ordered tabs
 * of cues from the global list, the same cue any number of times per page, stored
 * in Cues\CuePageInfo.Dat ("LJ4 cuepages 1", "page n name= cues=1,5,5,9"; the W1
 * reader/writer lj_cue_pages_load/save in lj_cue.h stays the file layer). This
 * header is the in-memory set the List of Cues' page tabs, the Pages menu (Add New
 * Page, Insert New Page, Delete Current Page, Edit Current Page Name, Clear Current
 * Page, Re-order Pages, Clear All Pages, Delete All Pages) and the Cue Page hotkeys
 * (Previous/Next Cue Page, Cue Page Latch/Select Previous/Next Cue) act on. */
#ifndef LJ_MODEL_CUEPAGE_H
#define LJ_MODEL_CUEPAGE_H

#include <stddef.h>
#include "model/lj_cue.h"

typedef struct { lj_cue_page page[LJ_CUE_PAGES]; int n; int current; int cursor; } lj_cuepages;   /* current = the shown tab (0-based), cursor = the selected cue index on it (-1 none) */

void lj_cuepages_init(lj_cuepages *p);                                          /* no pages, current 0, cursor -1 */
int  lj_cuepages_load(const char *path, lj_cuepages *out, char *err, size_t cap);   /* CuePageInfo.Dat through lj_cue_pages_load; a missing file = no pages (0); -1 only when the file exists and is bad */
int  lj_cuepages_save(const char *path, const lj_cuepages *in);
const char *lj_cuepages_validate(const lj_cuepages *in);                        /* NULL = valid: n <= LJ_CUE_PAGES, every cue number 1..LJ_NUMBER_MAX (lj_kv_range at load, ME-77), names NUL-terminated */
int  lj_cuepages_add(lj_cuepages *p, const char *name);                          /* Add New Page (appends); returns the index or -1 at LJ_CUE_PAGES */
int  lj_cuepages_insert(lj_cuepages *p, int before, const char *name);           /* Insert New Page before `before` */
int  lj_cuepages_delete(lj_cuepages *p, int index);                              /* Delete Current Page; current clamps */
int  lj_cuepages_rename(lj_cuepages *p, int index, const char *name);            /* Edit Current Page Name */
int  lj_cuepages_clear(lj_cuepages *p, int index);                               /* Clear Current Page: cues removed, page kept */
int  lj_cuepages_move(lj_cuepages *p, int from, int to);                         /* Re-order Pages */
int  lj_cuepages_add_cue(lj_cuepages *p, int index, uint32_t cue, int at /* -1 = append */);   /* Add cues to Page / Insert cues on Page / Save Cue "Add To Cuepage" (never adds twice [03 s5.7]: 0 when already present); -1 when the page is full */
int  lj_cuepages_remove_cue(lj_cuepages *p, int index, int at);
int  lj_cuepages_step(lj_cuepages *p, int delta, int circular);                  /* Previous/Next Cue Page (Circular Page Selection preference); the new current or -1 at an end */
uint32_t lj_cuepages_step_cue(lj_cuepages *p, int delta);                        /* Cue Page Select Previous/Next Cue: moves cursor on the current page; the cue number, 0 at an end */

#endif /* LJ_MODEL_CUEPAGE_H */
