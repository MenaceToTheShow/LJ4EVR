/* lj_defaultout.c - W2 contract stub (lj_defaultout.h); the model track replaces the bodies. */
#include <string.h>
#include "model/lj_defaultout.h"

int  lj_defaultout_use_scene(uint8_t out[LJ_CHANS], const lj_scene *scene) { (void)out; (void)scene; return -1; }
void lj_defaultout_clear(uint8_t out[LJ_CHANS]) { memset(out, 0, LJ_CHANS); }
int  lj_defaultout_scene_blocked(const lj_scene *scene) { (void)scene; return 0; }
