/* lj_genmacro.h - generic DMX macros, layer rank 6 (design 3.4, 3.6; 14 sA.3):
 * per channel a 16.16 index into an 8192-point shape, advanced once per tick,
 * delta = shape[(idx - delay) & 8191] * amp / 32640 on the reference (base or
 * the merged value of ranks 0-5), clamped 0..255 (16-bit on a declared pair). */
#ifndef LJ_ENGINE_GENMACRO_H
#define LJ_ENGINE_GENMACRO_H

#include <stdint.h>
#include "engine/lj_engine.h"

typedef struct { uint32_t idx; uint8_t back; } lj_gm_ch;   /* 16.16 index 0..8191, back = bounce running backwards */

int     lj_genmacro_programmed(const lj_genmacro *m, int ch);                   /* amp_pos or amp_neg non-zero (W2: the model's "no shape" encoding) */
int16_t lj_genmacro_shape(int shape, int k);                                    /* value at k (0..8191), +-32640 scale [14 sA.6]; W1: 0 '2*Sine' and 7 'Sine' from lj_sin4096, the others fall back to Sine (W2: lj_shapes_gen.c, the 66 exact tables) */
void    lj_genmacro_restart(lj_gm_ch st[LJ_CHANS]);
void    lj_genmacro_advance(const lj_genmacro *m, lj_gm_ch st[LJ_CHANS], uint32_t tick_us);
void    lj_genmacro_apply(const lj_genmacro *m, const lj_gm_ch st[LJ_CHANS], const lj_post_ctx *c, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS]);   /* rank 6 over merged[] */

#endif /* LJ_ENGINE_GENMACRO_H */
