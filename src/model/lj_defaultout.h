/* lj_defaultout.h - the Default DMX output (W2; 01 s6.3, design 3.4 rank 0): the
 * LJ_CHANS byte table the engine merges first (DefaultOut.dat, "LJ4 defaultout 1" +
 * "ch l:a=v" non-zero lines; the W1 reader/writer lj_defaultout_load/save in
 * lj_show.h stays the file layer). Setup > Preferences > Default DMX Output:
 * "Use Current Scene" records the current scene's programmed channels as new
 * defaults, merged over the existing ones; presets and pan/tilt macros in the scene
 * are an error; "Clear Default Output" resets to all 0. After either, the app posts
 * LJ_CMD_DEFAULT_OUT so the engine re-reads show->default_out (seam: the UI writes
 * the table under lj_lock, the engine reads it by pointer). Fixture Configuration
 * also writes a profile's default_dmx into it on save (LORD_FIXTURES-11, W1). */
#ifndef LJ_MODEL_DEFAULTOUT_H
#define LJ_MODEL_DEFAULTOUT_H

#include <stdint.h>
#include "model/lj_model.h"

int  lj_defaultout_use_scene(uint8_t out[LJ_CHANS], const lj_scene *scene);    /* every Snap/Fade channel of the scene overwrites out[]; -1 (nothing written) when lj_defaultout_scene_blocked [01 s6.3] */
void lj_defaultout_clear(uint8_t out[LJ_CHANS]);
int  lj_defaultout_scene_blocked(const lj_scene *scene);                        /* 1 when a preset reference or an active movement macro is in the scene (the error case of use_scene) */

#endif /* LJ_MODEL_DEFAULTOUT_H */
