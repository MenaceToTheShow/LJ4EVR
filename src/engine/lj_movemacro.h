/* lj_movemacro.h - movement macros (design 3.6, 17 s7): per fixture two 16.16
 * indexes into the LJ_MOVEMACRO_N_TABLE-point sine/cosine tables, advanced once
 * per tick in step 2 (ME-35) from the macro the previous tick's merged[] carried
 * for that fixture, reset when it carried none [17 s7.1]; runners only read it. */
#ifndef LJ_ENGINE_MOVEMACRO_H
#define LJ_ENGINE_MOVEMACRO_H

#include <stdint.h>
#include "model/lj_model.h"

typedef struct { uint32_t idx_pan, idx_tilt; } lj_mm_state;   /* 16.16 table index, wraps at LJ_MOVEMACRO_N_TABLE << 16 */

int  lj_movemacro_active(const lj_movemacro *m);                                                     /* type != No Macro (1..6 system, 7+ Bezier) with the type's fade state not Off */
void lj_movemacro_advance(lj_mm_state *s, const lj_movemacro *m, uint32_t tick_us, int speed_pct);  /* m NULL = reset; speed_pct = cue macro control (100 = raw) [03 s5.11] */
void lj_movemacro_offset(const lj_mm_state *s, const lj_movemacro *m, int amp_pct, int32_t *dpan, int32_t *dtilt);   /* 16-bit units: +-32768 at amplitude 127 [17 s7.3] */
int  lj_movemacro_scale(int raw, int pct);                                                           /* floor(raw * pct / 100) clamped 0..127 [17 s8] */

/* ---- W3 (docs/w3-contracts.md; movement track) ----
 * The pure position of one fixture at a phase, what tests/vectors.h checks against 17-position_ref.py section 7:
 * types 1..6 from lj_sin4096 / lj_cos4096, types >= LJ_MM_TYPE_BEZ0 from bez->t[type - 7] (NULL / out of range =
 * inactive); amp_pct = the cue macro control scale (100 = raw); base and result in readout units (17 s1.3), clamped to
 * -32768..32767 ("cut off at the edges"). 0, or -1 when the macro is inactive (*pan / *tilt = base). */
int lj_movemacro_pos(const lj_mm_state *s, const lj_movemacro *m, const lj_bez_set *bez, int amp_pct,
                     int32_t base_pan, int32_t base_tilt, int32_t *pan, int32_t *tilt);
/* the offset for every type (lj_movemacro_offset = this with bez NULL, where a Bezier type gives 0): what the runner
 * overlays with show->bez (movement track, W3) */
void lj_movemacro_offset_bez(const lj_mm_state *s, const lj_movemacro *m, const lj_bez_set *bez, int amp_pct, int32_t *dpan, int32_t *dtilt);

#endif /* LJ_ENGINE_MOVEMACRO_H */
