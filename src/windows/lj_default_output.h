/* lj_default_output.h - Setup > Preferences > Default DMX Output (01 s6.3, design 3.4 rank 0): the two menu
 * leaves "Use Current Scene" / "Clear Default Output". LJ2 has no dialog for them: TDefault2Form (DFM caption
 * "Set Fade Status": Intensity..Level/Extended boxes, All Scenes, the nine Fade Status transforms) is a
 * sequence tool, and TApplyDefaultDMXForm is Fixture Configuration's "Apply Default DMX values" progress box,
 * so LJ_WIN_DEFAULT_OUTPUT stays without a window (docs/w2-contracts.md UNVERIFIED item, settled from the
 * controls). Both leaves write show->default_out under lj_lock, post LJ_CMD_DEFAULT_OUT and save
 * DefaultOut.dat. A scene with presets or movement macros is refused with the exe's
 * "Default DMX Output cannot contain Presets" (0x30B238). */
#ifndef LJ_WINDOWS_DEFAULT_OUTPUT_H
#define LJ_WINDOWS_DEFAULT_OUTPUT_H

#include "windows/lj_windows.h"

int lj_default_output_run(int use_scene);   /* the LJ_ACT_DEFAULT_OUT arg: 1 = Use Current Scene (merged over the existing defaults), 0 = Clear; 0 / -1 (refused, nothing written) */

#endif /* LJ_WINDOWS_DEFAULT_OUTPUT_H */
