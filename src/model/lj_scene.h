/* lj_scene.h - scene alloc/copy/insert/delete/add (design 4 last paragraph, [F29]):
 * Insert is always before the current scene, delete-all leaves one all-off
 * scene, Add copies the current scene and appends, max LJ_SCENES_MAX. Every
 * mutation of a sequence the engine may be reading happens under lj_lock
 * (design 3.1); these functions do not take the lock. */
#ifndef LJ_MODEL_SCENE_H
#define LJ_MODEL_SCENE_H

#include "model/lj_model.h"

void lj_scene_init(lj_scene *s);                                 /* all Off, 0, scene_ms 1000, fade_ms 50, no presets, no macros */
void lj_scene_copy(lj_scene *dst, const lj_scene *src);
int  lj_scene_is_programmed(const lj_scene *s);                  /* any channel not LJ_OFF */

int  lj_sequence_alloc(lj_sequence *seq, uint16_t n);            /* scenes = n initialised scenes (n >= 1); frees any previous array; 0 / -1 */
void lj_sequence_free_scenes(lj_sequence *seq);                  /* n = 0, scenes = NULL */
int  lj_sequence_add(lj_sequence *seq, uint16_t current);        /* Add: append a copy of scene `current`; returns the new index or -1 at LJ_SCENES_MAX */
int  lj_sequence_insert(lj_sequence *seq, uint16_t before, uint16_t count, int copy_current);   /* Insert `count` scenes before index `before` (copies of it, or blank); returns the first new index or -1 */
int  lj_sequence_delete(lj_sequence *seq, uint16_t first, uint16_t last);   /* inclusive; deleting every scene leaves one all-off scene (lj_scene_init); 0 / -1 */
void lj_sequence_new(lj_sequence *seq);                          /* "New sequence": number 0, no name, one all-off scene, modified 0 */

#endif /* LJ_MODEL_SCENE_H */
