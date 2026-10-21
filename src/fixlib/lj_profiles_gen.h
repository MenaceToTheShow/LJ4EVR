/* lj_profiles_gen.h - the QLC+-derived tier-2 profiles that tools/qxf2c.py writes into
 * lj_profiles_gen.c (design 6.1 item 1). The profiles are mutable statics on purpose:
 * lj_layout_derive() fills windows / nwindows at the first lj_fixlib_by_id() (6.3), so the
 * database carries its own derived state and nothing else is global. */
#ifndef LJ_FIXLIB_PROFILES_GEN_H
#define LJ_FIXLIB_PROFILES_GEN_H

#include "fixlib/lj_profile.h"

enum { LJ_QXF_EXTRA_FIRST = 1000 };                   /* extras: fixtures or modes LJ2 never had, append-only ids from qxf_map.tsv (ME-8) */
extern lj_profile *const lj_qxf_profiles[];          /* sorted by id; a hand file for the same id wins in lj_fixlib_by_id */
extern const int lj_qxf_count;
extern const lj_exe_row lj_qxf_extra_rows[];         /* lj_fixlib_row(201 + i); hidden from the tree unless [FIXTURES] ShowExtraProfiles=1 */
extern const int lj_qxf_extra_count;

#endif /* LJ_FIXLIB_PROFILES_GEN_H */
