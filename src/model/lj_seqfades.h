/* lj_seqfades.h - Sequence fades [04 s1.4, CHM sequencefades.htm]: TDefault2Form "Set Fade Status" (the Fades menu of
 * every control). Changes only fade states, never values, of the selected fixtures' channels whose attribute is
 * checked, in the current scene or All Scenes; Movement Macro acts on lj_movemacro.fstate[]. A UI-side model edit
 * (the window posts LJ_CMD_CURRENT_SEQ after it, design 2.3); no engine command. Bodies: lj_seqfades.c
 * (gdc-fades-followspot track). */
#ifndef LJ_MODEL_SEQFADES_H
#define LJ_MODEL_SEQFADES_H

#include <stdint.h>
#include "model/lj_model.h"

enum lj_attr { LJ_ATTR_INTENSITY = 0, LJ_ATTR_MOVEMENT, LJ_ATTR_COLOR, LJ_ATTR_GOBO, LJ_ATTR_BEAM, LJ_ATTR_EFFECTS, LJ_ATTR_LEVEL,
               LJ_ATTR_MOVEMACRO, LJ_ATTR_COUNT };   /* the check boxes cbIntensity .. cbMovementmacro, in DFM order */
enum lj_sfx { LJ_SFX_ALL_OFF = 0, LJ_SFX_ALL_SNAP, LJ_SFX_ALL_FADE, LJ_SFX_OFF_SNAP, LJ_SFX_OFF_FADE, LJ_SFX_SNAP_OFF, LJ_SFX_SNAP_FADE,
              LJ_SFX_FADE_OFF, LJ_SFX_FADE_SNAP, LJ_SFX_COUNT };   /* RG1 "Fade Status" items, in order */
extern const char *const lj_attr_caption[LJ_ATTR_COUNT];   /* "Intensity" "Movement" "Color" "Gobo" "Beam" "Effects" "Level/Extended" "Movement Macro" */
extern const char *const lj_sfx_caption[LJ_SFX_COUNT];     /* "All Off" "All Snap" "All Fade" "Off -> Snap" "Off -> Fade" "Snap -> Off" "Snap -> Fade" "Fade -> Off" "Fade -> Snap" */
typedef struct { uint8_t attr_mask /* bit enum lj_attr */, all_scenes, sfx /* enum lj_sfx */; } lj_seqfades;

/* The one attribute of a profile offset: built-ins by channel class; user (.GS2) profiles in the help's priority order,
 * last wins: Movement/position, beam, effects, level (extended), intensity, gobo, color [CHM sequencefades.htm]. */
int lj_ch_attr(const lj_profile *p, int offset);   /* enum lj_attr, -1 = none */
int lj_seqfades_apply(const lj_seqfades *f, const lj_show *show, lj_sequence *seq, int scene, const uint8_t selected[LJ_FIXTURES]);   /* channels changed, -1 */

#endif /* LJ_MODEL_SEQFADES_H */
