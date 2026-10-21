/* lj_presets.h - Presets\fixtureNNN.pre (design 5.2: "LJ4 presets 1", "preset n
 * pan= tilt="): the 99 position presets of one fixture, pan/tilt only [F36],
 * readout units of 17 s1 (int16). NNN = fixture index 1..100. */
#ifndef LJ_MODEL_PRESETS_H
#define LJ_MODEL_PRESETS_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_presets_load(const char *path, lj_preset presets[LJ_PRESETS], char *err, size_t cap);
int         lj_presets_save(const char *path, const lj_preset presets[LJ_PRESETS]);
const char *lj_presets_validate(const lj_preset presets[LJ_PRESETS]);     /* NULL = valid (used 0/1) */

#endif /* LJ_MODEL_PRESETS_H */
