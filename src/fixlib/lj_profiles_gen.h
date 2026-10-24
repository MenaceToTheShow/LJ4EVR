/* lj_profiles_gen.h - the tier-2 profiles tools/qxf2c.py writes into lj_profiles_gen.c (one
 * lj_qxf_desc per mapped mode) and lj_profiles_gen_<k>.c (the encoded bytes), design 6.1 item 1,
 * W7. The descriptors are const; lj_fixlib_by_id() builds the lj_profile on the heap at first use
 * (lj_qxf_build in lj_profile.c, UI thread), keeps it in lj_qxf_built[] for the process lifetime
 * and lj_fixlib_release() frees it at exit. */
#ifndef LJ_FIXLIB_PROFILES_GEN_H
#define LJ_FIXLIB_PROFILES_GEN_H

#include "fixlib/lj_profile.h"

enum { LJ_QXF_EXTRA_FIRST = 1000 };                   /* extras: fixtures or modes LJ2 never had, append-only ids from qxf_map.tsv (ME-8) */
typedef struct {
    uint16_t id; uint8_t part; uint32_t at;            /* the encoded profile: lj_qxf_parts[part] + at */
    const char *tag, *manufacturer, *name, *mode;      /* identity: the exe row the map names, or the extra's own */
    const char *src, *src_mode;                        /* "QLC+ Martin-MAC500.qxf", "DMX4": the notes line */
} lj_qxf_desc;
extern const lj_qxf_desc lj_qxf_descs[];             /* sorted by id; a hand file for the same id wins in lj_fixlib_by_id */
extern const int lj_qxf_count;
extern const uint8_t *const lj_qxf_parts[];
extern lj_profile *lj_qxf_built[];                   /* [lj_qxf_count]: NULL until first use */
extern const lj_exe_row lj_qxf_extra_rows[];         /* lj_fixlib_row(201 + i), sorted by id */
extern const int lj_qxf_extra_count;

#endif /* LJ_FIXLIB_PROFILES_GEN_H */
