/* lj_followspot.h - the 20 follow spot settings [04 s13, F-inventory "Follow spot settings"] (lj_followspot in
 * lj_model.h, show->followspot[]). A setting is a recorded scene fragment plus the fixture selection, recalled into
 * the CURRENT SCENE with every channel snapped [11 F22]: it is not an engine layer and needs no engine command (the
 * UI writes the scene under lj_lock and posts LJ_CMD_CURRENT_SEQ, design 2.3); rank 6 generic macros stay on top
 * because they sit above the current sequence [11 C7]. LJ2 kept the settings in Follow2.Dat (binary, never imported,
 * design 5.3); ours is LJ_FOLLOWSPOT_FILE in the 5.2 grammar:
 *   LJ4 follow 1 / spot n used=1 name="Guitar Solo 1" sel=1,2,5 static=1.3 autoclr=1 / ch l:a=v:S ...
 * Bodies: lj_followspot.c (gdc-fades-followspot track). */
#ifndef LJ_MODEL_FOLLOWSPOT_H
#define LJ_MODEL_FOLLOWSPOT_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_model.h"

#define LJ_FOLLOWSPOT_FILE "FollowSpot.dat"   /* library root (PROPOSAL name; LJ2: Follow2.Dat) */
int         lj_followspot_load(const char *path, lj_followspot fs[LJ_FOLLOWSPOT], char *err, size_t cap);   /* missing file = 20 unused settings, 0 */
int         lj_followspot_save(const char *path, const lj_followspot fs[LJ_FOLLOWSPOT]);
const char *lj_followspot_validate(const lj_followspot fs[LJ_FOLLOWSPOT]);   /* static_page 0..64 / static_slot 0..LJ_STATICS2_SLOTS_MAX, names NUL-terminated, states <= LJ_FADE */
/* "Apply Current Scene to Preset#": every programmed channel of the selected fixtures and the selection itself. 0 / -1 (no selection). */
int lj_followspot_record(lj_followspot *fs, const lj_show *show, const lj_scene *sc, const uint8_t selected[LJ_FIXTURES]);
/* Activation: re-select the recorded fixtures (only = NULL) or, with "Use only selected fixtures as Follow spots", the
 * recorded fixtures that are in only[]; write their programmed channels as LJ_SNAP and every other channel of those
 * fixtures at the profile's default DMX as LJ_SNAP ("unprogrammed effects at default DMX"); sel_out[] = the new
 * selection. The caller then activates the associated static (LJ_CMD_STATIC_ACTIVATE) and opens Intensity + Movement.
 * Returns the number of fixtures applied, -1 when the setting is unused. */
int lj_followspot_apply(const lj_followspot *fs, const lj_show *show, lj_scene *sc, const uint8_t *only, uint8_t sel_out[LJ_FIXTURES]);

#endif /* LJ_MODEL_FOLLOWSPOT_H */
