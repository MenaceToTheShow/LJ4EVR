/* lj_groups.h - GroupInfo.dat (design 5.2: "LJ4 groups 1", "group n name=...
 * members=1,2,5"): the 99 user fixture groups [18 s8, 01 s3.4]. */
#ifndef LJ_MODEL_GROUPS_H
#define LJ_MODEL_GROUPS_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_groups_load(const char *path, lj_group groups[LJ_USER_GROUPS], char *err, size_t cap);
int         lj_groups_save(const char *path, const lj_group groups[LJ_USER_GROUPS]);
const char *lj_groups_validate(const lj_group groups[LJ_USER_GROUPS]);    /* NULL = valid: members 0/1, names NUL-terminated */


/* ---- W2 additions: HTP group names and Intensity Groups ----
 * HTP groups [05 s3.3, 11 P8]: membership is lj_patch_ch.htp_mask (bit k-1), levels are engine state
 * (LJ_CMD_HTP_GROUP); the 24 names live in GroupInfo.dat as "htp n name=" lines (ours: LJ2 keeps them
 * undecoded; unknown keys are skipped by the W1 reader, so the file stays readable by W1).
 * Intensity Groups (TIntensityGroupForm, 9 tabs): the sub-master a fixture belongs to is
 * lj_fixture.intensity_group (Config.dat sub=); the names are lj_show.submaster_name[] (5 chars + NUL). */
typedef struct { char name[LJ_HTP_GROUPS][LJ_NAME_CAP]; } lj_htp_names;
int  lj_htp_names_load(const char *path, lj_htp_names *out, char *err, size_t cap);   /* the "htp" lines of GroupInfo.dat; absent lines = "HTP group n" (PROPOSAL default) */
int  lj_htp_names_save(const char *path, const lj_group groups[LJ_USER_GROUPS], const lj_htp_names *names);   /* rewrites GroupInfo.dat with both line kinds */
int  lj_group_select(const lj_group *g, uint8_t selected[LJ_FIXTURES], int inclusive);   /* Fixture Groups window / hotkey Fixture group n: members -> selection (inclusive adds, else replaces); the member count */

#endif /* LJ_MODEL_GROUPS_H */
