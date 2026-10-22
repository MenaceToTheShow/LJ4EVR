/* lj_gdc.h - Generic DMX Control (TGenericMegaForm "Generic DMX Control", F35; CHM thegenericdmxcontrol.htm, 04 s8.4):
 * the keyboard command line parsed to a command list, run against the control's selection and the CURRENT SCENE
 * ("adds data into the current scene in the current sequence like any other fixture control"). Like every control it
 * writes the scene under lj_lock and posts LJ_CMD_CURRENT_SEQ; there is no engine command (design 2.3, 3.4).
 *
 * Grammar (CHM, verbatim operators): "n" selects channel n (the selection is cleared first unless the line starts
 * with "+"); "+n" adds, "-n" removes; "n/m" a range; "#g" group g ("#g/h" groups g..h; "#g/#h" is an input error);
 * "." and "," may stand for "#", "*" for "@"; "@v" sets the level, "@+v" / "@-v" relative; "@@v" programs ALL channels;
 * "##" selects all; o O s S f F set Off / Snap / Fade; an empty line clears the selection (when "Empty Command Clears
 * Selection" is on). Commands chain left to right: "#1/3F@50-#2@+10". Channel n = address code + 1 (1..2048: link 1
 * is 1..512; PROPOSAL rig F35: LJ2's map numbering is unverified); only channels of generic DMX profiles are mapped.
 * Levels are % (0..100, nearest to 0..255) with "Use % Values", else 0..255; results clamp to 0..255; a channel given
 * a level becomes Fade unless it is already Snap (CHM "the fade state of each channel will default to fade").
 * Bodies: lj_gdc.c (gdc-fades-followspot track). */
#ifndef LJ_MODEL_GDC_H
#define LJ_MODEL_GDC_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_model.h"

enum lj_gdc_op { LJ_GDC_SEL_SET = 0, LJ_GDC_SEL_ADD, LJ_GDC_SEL_REMOVE,      /* channels lo..hi */
                 LJ_GDC_GRP_SET, LJ_GDC_GRP_ADD, LJ_GDC_GRP_REMOVE,       /* groups lo..hi */
                 LJ_GDC_SEL_ALL /* ## */, LJ_GDC_SEL_NONE /* empty line */,
                 LJ_GDC_LEVEL /* @v */, LJ_GDC_LEVEL_UP /* @+v */, LJ_GDC_LEVEL_DOWN /* @-v */, LJ_GDC_LEVEL_ALL /* @@v */,
                 LJ_GDC_STATE /* o / s / f: v = lj_fstate */ };
enum { LJ_GDC_CMDS_MAX = 64, LJ_GDC_LINE_MAX = 128 };
typedef struct { uint8_t op; uint16_t lo, hi, v; } lj_gdc_cmd;   /* lo..hi 1-based channels or groups; v level or state */
typedef struct { int n; lj_gdc_cmd c[LJ_GDC_CMDS_MAX]; } lj_gdc_prog;
typedef struct { uint8_t sel[LJ_CHANS]; uint8_t percent /* Use % Values */, empty_clears /* Empty Command Clears Selection */; } lj_gdc_state;

int lj_gdc_parse(const char *line, lj_gdc_prog *out, char *err, size_t cap);   /* 0 / -1 + err ("input error at column n") */
int lj_gdc_is_generic(const lj_show *show, int code);                          /* the channel belongs to a generic DMX profile (the map) */
int lj_gdc_run(const lj_gdc_prog *p, const lj_show *show, lj_gdc_state *st, lj_scene *sc);   /* channels changed, -1 on a bad group / channel */
#define LJ_GDC_GROUPS_FILE "GenericGroups.dat"   /* library root, "LJ4 gdcgroups 1": group n name= / ch l:a groups=<mask> / hide l:a (PROPOSAL name) */
int lj_gdc_groups_load(const char *path, lj_gdc_groups *out, char *err, size_t cap);   /* missing file = no groups, 0 */
int lj_gdc_groups_save(const char *path, const lj_gdc_groups *in);

#endif /* LJ_MODEL_GDC_H */
