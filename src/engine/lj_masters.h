/* lj_masters.h - Grand Master / sub-master maths (design 3.5 S3, 11 G1-G5,
 * LORD_ENGINE-18) and the GM Fade Out/In runner (11 G11). */
#ifndef LJ_ENGINE_MASTERS_H
#define LJ_ENGINE_MASTERS_H

#include <stdint.h>
#include "engine/lj_engine_rules.h"

int lj_master_factor(int sub, int gm, int mode);                 /* k 0..255: one factor per enum lj_master_mode [11 G2] */
int lj_master_scale(int v, int k, int limit, int lo, int hi);    /* limit: lo + ((v-lo)*k + 127)/255 inside [lo, hi], unchanged outside [11 G4, G5]; else (v*k + 127)/255 [11 G1], nearest */

typedef struct { uint8_t mode; uint64_t t0_us, dur_us; uint8_t from, to, restore; } lj_gm_fade;   /* mode 0 none, 1 out, 2 in (the view's gm_fade) */
void lj_gm_fade_start(lj_gm_fade *f, int in, uint32_t ms, uint8_t gm_now, uint64_t now_us);   /* ms clamped to LJ_GM_FADE_MIN/MAX_MS; a new time does not affect an active fade [11 G11] */
int  lj_gm_fade_step(lj_gm_fade *f, uint64_t now_us, uint8_t *gm);                            /* moves *gm, returns the mode (0 once done) */
void lj_gm_fade_cancel(lj_gm_fade *f);                                                        /* a SET_MASTER on the GM ends the fade */

#endif /* LJ_ENGINE_MASTERS_H */
